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

#ifndef QUERYTABITEM_H
#define QUERYTABITEM_H

#include <fx.h>

#include "SqlConnection.h"

// A FXTabItem is rather simple, and is essentially just a label. New controls
// are not really bound to the TabItem but the tabbook itself. It's not clear
// how FOX determines what elements are part of which tabs.

class QueryTabItem : public FXTabItem {
  FXDECLARE(QueryTabItem)
public:
  QueryTabItem(FXTabBook *tabbook, const FXString& label, tds::SqlConnection *conn);
  virtual ~QueryTabItem() = default;

  virtual void create();

  void ExecuteQuery();
private:
  QueryTabItem() = default;

  FXTabBook *parent;
  FXText *text;

  FXHorizontalFrame *frame;

  tds::SqlConnection *conn;
};

#endif // QUERYTABITEM_H

