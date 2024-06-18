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

#ifndef SERVEREDITDLG_H
#define SERVEREDITDLG_H

#include <fx.h>

#include "Server.h"

class ServerEditDialog : public FXDialogBox {
  FXDECLARE(ServerEditDialog);
public:
  explicit ServerEditDialog(FXWindow *owner, const Server *server);
  virtual ~ServerEditDialog();
  enum {
    ID_ACCEPT = FXTopWindow::ID_LAST,
    ID_CANCEL,
  };

  long OnAccept(FXObject*,FXSelector,void*);
  long OnCancel(FXObject*,FXSelector,void*);

  [[nodiscard]] FXString name() const { return m_name->getText().trim(); }
  [[nodiscard]] FXString host() const { return m_hostname->getText().trim(); }
  [[nodiscard]] FXint port() const { return m_port->getValue(); };
  [[nodiscard]] FXString instance() const { return m_instance->getText().trim(); }
  [[nodiscard]] FXString username() const { return m_username->getText().trim(); }
  [[nodiscard]] FXString password() const { return m_password->getText().trim(); }
  [[nodiscard]] FXString database() const { return m_database->getText().trim(); }

private:
  ServerEditDialog() = default;

  FXLabel *m_name_lbl;
  FXTextField *m_name;
  FXLabel *m_hostname_lbl;
  FXTextField *m_hostname;
  FXSpinner *m_port;
  FXTextField *m_instance;

  FXLabel *m_username_lbl;
  FXTextField *m_username;
  FXLabel *m_password_lbl;
  FXTextField *m_password;
  FXTextField *m_database;

  FXLabel *m_error;
};

#endif // SERVEREDITDLG_H

