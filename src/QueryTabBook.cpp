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

#include "QueryTabBook.h"

FXDEFMAP(QueryTabBook) queryTabBookMap[] = {
};


FXIMPLEMENT(QueryTabBook, FXTabBook, queryTabBookMap, ARRAYNUMBER(queryTabBookMap))

QueryTabBook::QueryTabBook(FXComposite *parent) :
  FXTabBook(parent, nullptr, 0, LAYOUT_FILL_X | LAYOUT_FILL_Y)
{
}

void QueryTabBook::AddTab(const FXString& label, tds::SqlConnection *conn)
{
  QueryTabItem *newTab = new QueryTabItem(this, label, conn);
  newTab->create();
  newTab->show();
}

void QueryTabBook::ExecuteActiveTabQuery()
{
  // Get current tab
  int tabIndex = this->getCurrent();
  if (tabIndex == -1) {
    // No selected tab
    return;
  }

  auto *item = static_cast<QueryTabItem *>(this->childAtIndex(tabIndex * 2));
  if (item == nullptr) {
    fprintf(stderr, "Failed to find query tab, can't run query!\n");
    return;
  }

  printf("Running a query on %d... %s\n", tabIndex, item->getText().text());
  item->ExecuteQuery();
}
