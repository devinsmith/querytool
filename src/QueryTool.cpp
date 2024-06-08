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
#include <FXICOIcon.h>

#include "QueryTool.h"

FXDEFMAP(QueryTool) queryToolMap[] = {
  FXMAPFUNC(SEL_COMMAND, QueryTool::ID_ABOUT, QueryTool::OnCommandAbout),
  FXMAPFUNC(SEL_COMMAND, QueryTool::ID_CONNECT, QueryTool::OnCommandConnect),
  FXMAPFUNC(SEL_COMMAND, QueryTool::ID_PREFERENCES, QueryTool::OnCommandPreferences),
  FXMAPFUNC(SEL_COMMAND, QueryTool::ID_QUIT, QueryTool::OnCommandQuit),
  FXMAPFUNC(SEL_COMMAND, QueryTool::ID_TEST_QUERY, QueryTool::OnCommandTestQuery)
};

FXIMPLEMENT(QueryTool, FXMainWindow, queryToolMap, ARRAYNUMBER(queryToolMap))

QueryTool::QueryTool(FXApp *app) :
    FXMainWindow(app, "SQL Query Tool", nullptr, nullptr, DECOR_ALL, 0, 0, 800, 600),
    tabBook{nullptr}
{
  menuBar = new FXMenuBar(this, LAYOUT_SIDE_TOP | LAYOUT_FILL_X);

  // File menu
  menuPanes[0] = new FXMenuPane(this);
  m_file_connect = new FXMenuCommand(menuPanes[0], "Connect...", nullptr, this,
      ID_CONNECT);
  m_file_disconnect = new FXMenuCommand(menuPanes[0], "Disconnect", nullptr,
      this, ID_DISCONNECT);
  m_file_disconnect->disable();
  m_file_sep = new FXMenuSeparator(menuPanes[0]);
  m_file_quit = new FXMenuCommand(menuPanes[0], "Quit\tCtrl-Q", nullptr, this, ID_QUIT);
  menuTitle[0] = new FXMenuTitle(menuBar, "&File", nullptr, menuPanes[0]);

  // Edit menu
  menuPanes[1] = new FXMenuPane(this);
  m_edit_pref = new FXMenuCommand(menuPanes[1], "&Preferences", nullptr, this, ID_PREFERENCES);
  menuTitle[1] = new FXMenuTitle(menuBar, "&Edit", nullptr, menuPanes[1]);

  // Help menu
  menuPanes[2] = new FXMenuPane(this);
  m_help_about = new FXMenuCommand(menuPanes[2], "&About...", nullptr, this, ID_ABOUT);
  menuTitle[2] = new FXMenuTitle(menuBar, "&Help", nullptr, menuPanes[2]);

  // Test menu
  menuPanes[3] = new FXMenuPane(this);
  m_test_show_query = new FXMenuCommand(menuPanes[3], "Show test query", nullptr, this, ID_TEST_QUERY);
  menuTitle[3] = new FXMenuTitle(menuBar, "&Test", nullptr, menuPanes[3]);

  FXSplitter *splitter = new FXSplitter(this, LAYOUT_SIDE_TOP | LAYOUT_FILL_X | LAYOUT_FILL_Y);

  FXVerticalFrame *srvFrame = new FXVerticalFrame(splitter, FRAME_SUNKEN | FRAME_THICK |
      LAYOUT_FILL_X | LAYOUT_FILL_Y, 0,0, 200,0, 0,0,0,0);
  queryFrame = new FXVerticalFrame(splitter, FRAME_SUNKEN|FRAME_THICK|LAYOUT_FILL_X|LAYOUT_FILL_Y);
//  queryFrame->setBackColor(FXRGB(128,128,128));

#if 1
  treeList = new FXTreeList(srvFrame, nullptr, 0, LAYOUT_FILL_Y | LAYOUT_FILL_X | TREELIST_SINGLESELECT | TREELIST_SHOWS_LINES | TREELIST_SHOWS_BOXES,
        0, 0, 200, 0);

  // Populate tree view with a root item
  FXTreeItem *rootItem = new FXTreeItem("SQL Connections");
  treeList->appendItem(nullptr, rootItem);
#endif
#if 1
  // Create the tab book on the right
  tabBook = new FXTabBook(queryFrame, nullptr, 0, LAYOUT_FILL_X | LAYOUT_FILL_Y);
  new FXTabItem(tabBook, "Testing", NULL);
#endif
  // Initial empty tab
  FXHorizontalFrame *frame = new FXHorizontalFrame(tabBook, FRAME_THICK|FRAME_RAISED);
  new FXText(frame, nullptr, 0, LAYOUT_FILL_X | LAYOUT_FILL_Y);

}

QueryTool::~QueryTool()
{
  for (auto pane : menuPanes) {
    delete pane;
  }

  delete treeList;
}

void
QueryTool::create()
{
  FXMainWindow::create();

  show(PLACEMENT_SCREEN);
}

long QueryTool::OnCommandAbout(FXObject*, FXSelector, void*)
{
  FXDialogBox about(this, "About SQL Query Tool", DECOR_TITLE | DECOR_BORDER);
  FXVerticalFrame *content = new FXVerticalFrame(&about, LAYOUT_SIDE_LEFT |
      LAYOUT_FILL_X | LAYOUT_FILL_Y);

  new FXLabel(content, "SQL Query Tool 0.0.1", nullptr, JUSTIFY_LEFT | LAYOUT_FILL_X |
      LAYOUT_FILL_Y);
  new FXLabel(content, "Copyright (C) 2024 Devin Smith (devin@devinsmith.net)", nullptr, JUSTIFY_LEFT |
  LAYOUT_FILL_X |
      LAYOUT_FILL_Y);

  // Ok button.
  FXButton *button = new FXButton(content, "OK", nullptr, &about, FXDialogBox::ID_ACCEPT, BUTTON_INITIAL |
  BUTTON_DEFAULT | FRAME_RAISED | FRAME_THICK | LAYOUT_CENTER_X, 0, 0, 0, 0, 32, 32, 5, 5);
  button->setFocus();

  about.execute(PLACEMENT_OWNER);
  return 1;
}

long QueryTool::OnCommandConnect(FXObject*, FXSelector, void*)
{
  return 1;
}

long QueryTool::OnCommandDisconnect(FXObject*, FXSelector, void*)
{
  return 1;
}

long QueryTool::OnCommandPreferences(FXObject*, FXSelector, void*)
{
  return 1;
}

long QueryTool::OnCommandQuit(FXObject*, FXSelector, void*)
{
  getApp()->exit(0);
  return 1;
}

long QueryTool::OnCommandTestQuery(FX::FXObject *, FX::FXSelector, void *)
{
  printf("Please work!\n");
  FXTabItem *blah = new FXTabItem(tabBook, "Additional tab", NULL);
#if 0
  FXTabItem *newTab;
  if (tabBook == nullptr) {
    tabBook = new FXTabBook(queryFrame, nullptr, 0, LAYOUT_FILL_X | LAYOUT_FILL_Y);
    new FXTabItem(tabBook, "Brand new", NULL);
  } else {
    new FXTabItem(tabBook, "Additional tab", NULL);
  }
#endif
  FXHorizontalFrame *frame = new FXHorizontalFrame(tabBook, FRAME_THICK|FRAME_RAISED);
  new FXText(frame, nullptr, 0, LAYOUT_FILL_X | LAYOUT_FILL_Y);

  printf("%d\n", blah->shown());

  blah->create();
//  frame->create();
  blah->show();
  tabBook->recalc();
  return 1;
}

