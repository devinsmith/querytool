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

#ifndef QUERYTABBOOK_H
#define QUERYTABBOOK_H

#include <fx.h>
#include <list>

#include "QueryTabItem.h"
#include "SqlConnection.h"

class QueryTabBook : public FXTabBook {
  FXDECLARE(QueryTabBook)
public:
  QueryTabBook(FXComposite *parent);
  virtual ~QueryTabBook() = default;

  void AddTab(const FXString& label, tds::SqlConnection *conn);
private:
  QueryTabBook() = default;
};

#endif // QUERYTABBOOK_H

