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


#include <fxkeys.h>

#include "ServerEditDlg.h"

FXDEFMAP(ServerEditDialog) ServerEditDialogMap[] = {
  FXMAPFUNC(SEL_COMMAND, ServerEditDialog::ID_ACCEPT, ServerEditDialog::OnAccept),
  FXMAPFUNC(SEL_COMMAND, ServerEditDialog::ID_CANCEL, ServerEditDialog::OnCancel),
};

FXIMPLEMENT(ServerEditDialog, FXDialogBox, ServerEditDialogMap,
    ARRAYNUMBER(ServerEditDialogMap))

ServerEditDialog::ServerEditDialog(FXWindow *owner, const Server *server) :
  FXDialogBox(owner, "Add/Edit Server...", DECOR_TITLE | DECOR_BORDER,
    0,0,0,0, 0,0,0,0, 0,0)
{
  FXVerticalFrame *contents = new FXVerticalFrame(this,
      LAYOUT_SIDE_LEFT | LAYOUT_FILL_X | LAYOUT_FILL_Y,
      0,0,0,0, 10,10,10,10,
      0,0);

  FXMatrix *matrix = new FXMatrix(contents, 2, MATRIX_BY_COLUMNS |
      LAYOUT_SIDE_TOP | LAYOUT_FILL_X | LAYOUT_FILL_Y);

  m_name_lbl = new FXLabel(matrix, "Name:", nullptr, JUSTIFY_LEFT | LAYOUT_FILL_COLUMN |
                                        LAYOUT_FILL_ROW);
  m_name = new FXTextField(matrix, 25, nullptr, 0, FRAME_THICK| FRAME_SUNKEN|LAYOUT_FILL_COLUMN|LAYOUT_FILL_ROW);

  m_hostname_lbl = new FXLabel(matrix, "Hostname:", nullptr, JUSTIFY_LEFT | LAYOUT_FILL_COLUMN |
      LAYOUT_FILL_ROW);
  m_hostname = new FXTextField(matrix, 25, nullptr, 0,
                                    FRAME_THICK|FRAME_SUNKEN|LAYOUT_FILL_COLUMN|LAYOUT_FILL_ROW);

  new FXLabel(matrix, "Port:", nullptr, JUSTIFY_LEFT|LAYOUT_FILL_COLUMN|LAYOUT_FILL_ROW);
  m_port = new FXSpinner(matrix, 23, nullptr, 0, FRAME_SUNKEN|FRAME_THICK|LAYOUT_FILL_COLUMN|LAYOUT_FILL_ROW);
  m_port->setRange(0, 65536);
  m_port->setValue(1433);

  new FXLabel(matrix, "Instance:", nullptr, JUSTIFY_LEFT | LAYOUT_FILL_COLUMN |
                                            LAYOUT_FILL_ROW);
  m_instance = new FXTextField(matrix, 25, nullptr, 0,
                               FRAME_THICK|FRAME_SUNKEN|LAYOUT_FILL_COLUMN|LAYOUT_FILL_ROW);

  m_username_lbl = new FXLabel(matrix, "Username:", nullptr, JUSTIFY_LEFT|LAYOUT_FILL_COLUMN|LAYOUT_FILL_ROW);
  m_username = new FXTextField(matrix, 25, NULL, 0, TEXTFIELD_ENTER_ONLY|FRAME_SUNKEN|FRAME_THICK|LAYOUT_FILL_COLUMN|LAYOUT_FILL_ROW);
  m_username->setText(FXSystem::currentUserName());

  m_password_lbl = new FXLabel(matrix, "Password:", nullptr, JUSTIFY_LEFT | LAYOUT_FILL_COLUMN |
                                            LAYOUT_FILL_ROW);
  m_password = new FXTextField(matrix, 25, nullptr, 0,
                               TEXTFIELD_ENTER_ONLY | TEXTFIELD_PASSWD |FRAME_THICK|FRAME_SUNKEN|LAYOUT_FILL_COLUMN|LAYOUT_FILL_ROW);

  new FXLabel(matrix, "Default database:", nullptr, JUSTIFY_LEFT | LAYOUT_FILL_COLUMN |
                                            LAYOUT_FILL_ROW);
  m_database = new FXTextField(matrix, 25, nullptr, 0,
                               FRAME_THICK|FRAME_SUNKEN|LAYOUT_FILL_COLUMN|LAYOUT_FILL_ROW);

  m_error = new FXLabel(contents, " ");

  FXHorizontalFrame *buttonframe = new FXHorizontalFrame(contents,LAYOUT_FILL_X|LAYOUT_FILL_Y);
  new FXButton(buttonframe, "&OK", nullptr, this, ServerEditDialog::ID_ACCEPT,
               BUTTON_INITIAL|BUTTON_DEFAULT|FRAME_RAISED|FRAME_THICK|LAYOUT_CENTER_X, 0,0,0,0, 32,32,5,5);
  new FXButton(buttonframe, "Cancel", nullptr, this, ServerEditDialog::ID_CANCEL,
               BUTTON_DEFAULT|FRAME_RAISED|FRAME_THICK|LAYOUT_CENTER_X, 0,0,0,0, 32,32,5,5);

  if (server != nullptr) {
    m_name->setText(server->name);
    m_hostname->setText(server->server);
    // Server name could be very long
    m_hostname->setCursorPos(0);
    m_port->setValue(server->port);
    m_instance->setText(server->instance);
    m_username->setText(server->user);
    m_password->setText(server->password);
    m_database->setText(server->default_database);
  }
}

ServerEditDialog::~ServerEditDialog()
{
}

long ServerEditDialog::OnAccept(FXObject*, FXSelector, void*)
{
  bool anyError = false;
  m_name_lbl->setTextColor(FXRGB(0, 0, 0));
  m_hostname_lbl->setTextColor(FXRGB(0, 0, 0));
  m_username_lbl->setTextColor(FXRGB(0, 0, 0));
  m_password_lbl->setTextColor(FXRGB(0, 0, 0));

  if (m_name->getText().trim().empty()) {
    anyError = true;
    m_name_lbl->setTextColor(FXRGB(255, 0, 0));
    m_name->setText("");
  }

  if (m_hostname->getText().trim().empty()) {
    anyError = true;
    m_hostname_lbl->setTextColor(FXRGB(255, 0, 0));
    m_hostname->setText("");
  }

  if (m_username->getText().trim().empty()) {
    anyError = true;
    m_username_lbl->setTextColor(FXRGB(255, 0, 0));
    m_username->setText("");
  }

  if (m_password->getText().trim().empty()) {
    anyError = true;
    m_password_lbl->setTextColor(FXRGB(255, 0, 0));
    m_password->setText("");
  }

  if (anyError) {
    m_error->setText("Please fill out the required fields!");
    return 1;
  }

  getApp()->stopModal(this, TRUE);
  hide();
  return 1;
}

long ServerEditDialog::OnCancel(FXObject* obj, FXSelector sel, void* ud)
{
  getApp()->stopModal(this, FALSE);
  hide();
  return 1;
}

