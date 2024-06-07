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

#ifndef QUERYTOOL_H
#define QUERYTOOL_H

#include <fx.h>

class QueryTool : public FXMainWindow {
  FXDECLARE(QueryTool)
public:
  QueryTool(FXApp *app);
  virtual ~QueryTool();
  enum {
    ID_ABOUT = FXMainWindow::ID_LAST,
    ID_QUIT,
    ID_CONNECT,
    ID_DISCONNECT,
    ID_PREFERENCES
  };

  void create();

  long OnCommandAbout(FXObject*, FXSelector, void*);
  long OnCommandConnect(FXObject*, FXSelector, void*);
  long OnCommandDisconnect(FXObject*, FXSelector, void*);
  long OnCommandPreferences(FXObject*, FXSelector, void*);
  long OnCommandQuit(FXObject*, FXSelector, void*);
private:
  QueryTool() {}

  FXTabBook *tabBook;
  FXTreeList *treeList;

  FXMenuPane *menuPanes[3];
  FXMenuTitle *menuTitle[3];

  FXMenuBar *menuBar;

  // File menu commands
  FXMenuCommand *m_file_connect;
  FXMenuCommand *m_file_disconnect;
  FXMenuSeparator *m_file_sep;
  FXMenuCommand *m_file_quit;

  // Edit
  FXMenuCommand *m_edit_pref;

  // Help
  FXMenuCommand *m_help_about;
};

#endif // QUERYTOOL_H

