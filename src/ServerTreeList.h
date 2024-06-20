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

#ifndef SERVERTREELIST_H
#define SERVERTREELIST_H

#include <fx.h>
#include <list>

#include "Server.h"

class ServerTreeList : public FXTreeList {
  FXDECLARE(ServerTreeList)
public:
  explicit ServerTreeList(FXComposite *parent);
  virtual ~ServerTreeList();

  virtual void create();

  enum {
    ID_REQUEST_TREE = FXTreeList::ID_LAST,
    ID_NEW,
    ID_CONNECT,
    ID_DISCONNECT,
    ID_EDIT,
    ID_DELETE
  };

  // Events.
  long OnCmdTreeLeftClick(FXObject*, FXSelector, void*);
  long OnCmdTreeRightClick(FXObject*, FXSelector, void*);
  long OnAddNewServer(FXObject*, FXSelector, void*);
  long OnEditServer(FXObject*, FXSelector, void*);
  long OnDeleteServer(FXObject*, FXSelector, void*);
private:
  ServerTreeList() = default;

  void loadConfig();
  void saveConfig() const;

  // Request tree view icons.
  FXIcon *ico_root;
  FXIcon *ico_server;
  FXTreeItem *m_root;

  std::list<Server> ServerList;
};

#endif // SERVERTREELIST_H
