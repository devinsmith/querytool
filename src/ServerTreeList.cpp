//
// Copyright (c) 2024 Devin Smith <devin@devinsmith.net>
//
// Permission to use, copy, modify, and distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice appear in all copies.
//
// THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
// WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
// ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
// WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
// ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
// OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
//

#include <fx.h>
#include <fstream>
#include <sstream>
#include <cstdio>
#include "Config.h"
#include "ServerEditDlg.h"
#include "ServerTreeList.h"
#include "icons/root.xpm"
#include "icons/server.xpm"

#include <cJSON.h>

FXDEFMAP(ServerTreeList) stlEventMap[] = {
  FXMAPFUNC(SEL_LEFTBUTTONPRESS, ServerTreeList::ID_REQUEST_TREE, ServerTreeList::OnCmdTreeLeftClick),
  FXMAPFUNC(SEL_RIGHTBUTTONPRESS, ServerTreeList::ID_REQUEST_TREE, ServerTreeList::OnCmdTreeRightClick),
  FXMAPFUNC(SEL_COMMAND, ServerTreeList::ID_NEW, ServerTreeList::OnAddNewServer),
  FXMAPFUNC(SEL_COMMAND, ServerTreeList::ID_EDIT, ServerTreeList::OnEditServer),
  FXMAPFUNC(SEL_COMMAND, ServerTreeList::ID_DELETE, ServerTreeList::OnDeleteServer)
};

FXIMPLEMENT(ServerTreeList, FXTreeList, stlEventMap, ARRAYNUMBER(stlEventMap))

ServerTreeList::ServerTreeList(FXComposite *parent) :
  FXTreeList(parent, this, ID_REQUEST_TREE,
      FRAME_SUNKEN | FRAME_THICK | LAYOUT_FILL_X | LAYOUT_FILL_Y |
      LAYOUT_TOP | LAYOUT_RIGHT | TREELIST_SHOWS_BOXES | TREELIST_SHOWS_LINES |
      TREELIST_SINGLESELECT)
{
  ico_root = new FXXPMIcon(getApp(), root_xpm);
  ico_server = new FXXPMIcon(getApp(), server_xpm);
}

ServerTreeList::~ServerTreeList()
{
  saveConfig();

  // Cleanup icons
  delete ico_root;
  delete ico_server;
}

void ServerTreeList::create()
{
  FXTreeList::create();

  ico_root->create();
  ico_server->create();

  m_root = appendItem(nullptr, "Servers", ico_root, ico_root);
  m_root->setHasItems(true);

  // Load servers resource
  loadConfig();

  for (auto& server : ServerList) {
    FXString label = server.name + " (";
    label += server.user;
    label += ")";

    appendItem(m_root, label, ico_server, ico_server, &server);
  }

  if (!ServerList.empty()) {
    expandTree(m_root, false);
  }

}

long ServerTreeList::OnAddNewServer(FX::FXObject *, FX::FXSelector, void *)
{
  ServerEditDialog editDlg(this, nullptr);
  if (editDlg.execute(PLACEMENT_OWNER)) {
    Server temp;
    ServerList.push_back(temp);
    Server* server = &ServerList.back();

    server->name = editDlg.name();
    server->server = editDlg.host();
    server->port = editDlg.port();
    server->instance = editDlg.instance();
    server->user = editDlg.username();
    server->password = editDlg.password();
    server->default_database = editDlg.database();

    FXString label = server->name + " (";
    label += server->user;
    label += ")";

    appendItem(m_root, label, ico_server, ico_server, server);
    expandTree(m_root, true);
  }
  return 1;
}

long ServerTreeList::OnEditServer(FX::FXObject *, FX::FXSelector, void *)
{
  // Get selected item:
  FXTreeItem *item = getCurrentItem();
  Server *server = nullptr;
  if (item != nullptr) {
    server = static_cast<Server *>(item->getData());
  }

  ServerEditDialog editDlg(this, server);
  if (editDlg.execute(PLACEMENT_OWNER)) {
    server->name = editDlg.name();
    server->server = editDlg.host();
    server->port = editDlg.port();
    server->instance = editDlg.instance();
    server->user = editDlg.username();
    server->password = editDlg.password();
    server->default_database = editDlg.database();

    FXString label = server->name + " (";
    label += server->user;
    label += ")";

    item->setText(label);

    updateItem(item);
  }
  return 1;
}

long ServerTreeList::OnDeleteServer(FX::FXObject *, FX::FXSelector, void *)
{
  // Get selected item:
  FXTreeItem *item = getCurrentItem();
  Server *server = nullptr;
  if (item != nullptr) {
    server = static_cast<Server *>(item->getData());
  }

  for (std::list<Server>::iterator it = ServerList.begin(); it != ServerList.end();) {
    if (&(*it) == server) {
      it = ServerList.erase(it);
    } else {
      it++;
    }
  }

  removeItem(item);
  return 1;
}

long ServerTreeList::OnCmdTreeLeftClick(FXObject *obj, FXSelector, void* ptr)
{
  FXEvent* event=(FXEvent*)ptr;
  FXTreeItem *item;
  FXint code;

  printf("Got here!\n");

  // Locate item
  item=getItemAt(event->win_x,event->win_y);
  if (item == nullptr) {
    killSelection(true);
    return 1;
  }

  // Find out where hit
  code=hitItem(item,event->win_x,event->win_y);
  printf("code = %d\n", code);

  setCurrentItem(item, true);
  // Change item selection
  state=item->isSelected();
  if (item->isEnabled() && !state) selectItem(item, true);
  flags |= FLAG_PRESSED;
  return 1;
}

long ServerTreeList::OnCmdTreeRightClick(FXObject* obj, FXSelector, void* ptr)
{
  const FXEvent* event = (FXEvent*)ptr;
  FXTreeItem *item = getItemAt(event->click_x, event->click_y);

  FXMenuPane serverMenu(this);

  if (!item) {
    new FXMenuCommand(&serverMenu, "Add new server...", nullptr, this, ID_NEW);
  } else {
    setCurrentItem(item, true);
    state=item->isSelected();
    if (item->isEnabled() && !state) selectItem(item, true);
    new FXMenuCommand(&serverMenu, "Connect to server", nullptr, this, ID_CONNECT);
    new FXMenuCommand(&serverMenu, "Disconnect", nullptr, this, ID_DISCONNECT);
    new FXMenuCommand(&serverMenu, "Edit server...", nullptr, this, ID_EDIT);
    new FXMenuCommand(&serverMenu, "Delete Server", nullptr, this, ID_DELETE);
  }

  serverMenu.create();
  serverMenu.popup(nullptr, event->root_x, event->root_y);
  getApp()->runModalWhileShown(&serverMenu);
  return 1;
}

static FXString getJSONString(cJSON *root, const char *property)
{
  const cJSON *obj = cJSON_GetObjectItem(root, property);
  if (obj != nullptr && obj->type == cJSON_String) {
    return obj->valuestring;
  }
  return {};
}

static int getJSONInt(cJSON *root, const char *property)
{
  const cJSON *obj = cJSON_GetObjectItem(root, property);
  if (obj != nullptr && obj->type == cJSON_Number) {
    return obj->valueint;
  }
  return 0;
}

void ServerTreeList::loadConfig()
{
  FXString serverConfig = Config::instance().dir() + "/servers.json";

  std::ifstream file(serverConfig.text());
  if (!file) {
    return;
  }

  // Read entire file into string buffer.
  std::stringstream ss;
  ss << file.rdbuf();
  std::string s(ss.str());

  cJSON *json = cJSON_Parse(s.c_str());
  if (json == nullptr) {
    // Parsing of an invalid file.
    return;
  }

  if (json->type != cJSON_Array) {
    // Not an array.
    return;
  }

  int num_servers = cJSON_GetArraySize(json);
  for (int i = 0; i < num_servers; i++) {
    cJSON *jsonServer = cJSON_GetArrayItem(json, i);

    Server temp;
    ServerList.push_back(temp);
    Server* server = &ServerList.back();

    // Extract JSON properties
    server->name = getJSONString(jsonServer, "name");
    server->server = getJSONString(jsonServer, "server");
    server->port = getJSONInt(jsonServer, "port");
    server->instance = getJSONString(jsonServer, "instance");
    server->user = getJSONString(jsonServer, "user");
    server->password = getJSONString(jsonServer, "password");
    server->default_database = getJSONString(jsonServer, "database");
  }

  cJSON_Delete(json);
}

void ServerTreeList::saveConfig() const
{
  printf("Saving config!\n");
  FXString serverConfig = Config::instance().dir() + "/servers.json";

  FILE *fp = fopen(serverConfig.text(), "w");
  if (fp == nullptr) {
    fprintf(stderr, "Failed to saveConfig\n");
    return;
  }

  cJSON *json = cJSON_CreateArray();

  for (const auto& server : ServerList) {
    cJSON *jsonServer = cJSON_CreateObject();
    cJSON_AddStringToObject(jsonServer, "name", server.name.text());
    cJSON_AddStringToObject(jsonServer, "server", server.server.text());
    cJSON_AddNumberToObject(jsonServer, "port", server.port);
    cJSON_AddStringToObject(jsonServer, "instance", server.instance.text());
    cJSON_AddStringToObject(jsonServer, "user", server.user.text());
    cJSON_AddStringToObject(jsonServer, "password", server.password.text());
    cJSON_AddStringToObject(jsonServer, "database", server.default_database.text());

    cJSON_AddItemToArray(json, jsonServer);
  }

  char *raw = cJSON_Print(json);
  cJSON_Delete(json);

  fwrite(raw, 1, strlen(raw), fp);

  free(raw);
  fclose(fp);
}
