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

#include "QueryTabItem.h"

FXDEFMAP(QueryTabItem) queryTabItemMap[] = {
};

FXIMPLEMENT(QueryTabItem, FXTabItem, queryTabItemMap, ARRAYNUMBER(queryTabItemMap))

QueryTabItem::QueryTabItem(FXTabBook *tabbook, const FXString& label, tds::SqlConnection *conn) :
  FXTabItem(tabbook, label, nullptr), parent(tabbook), conn{conn}
{
  frame = new FXHorizontalFrame(parent, FRAME_THICK|FRAME_RAISED);

  FXSplitter *splitter = new FXSplitter(frame, SPLITTER_VERTICAL | LAYOUT_FILL_X | LAYOUT_FILL_Y);
  FXVerticalFrame *queryTextFrame = new FXVerticalFrame(splitter, FRAME_SUNKEN | FRAME_THICK |
      LAYOUT_FILL_X | LAYOUT_FILL_Y, 0,0, 0, 0, 0,0,0,0);
  text = new FXText(queryTextFrame, nullptr, 0, LAYOUT_FILL_X | LAYOUT_FILL_Y);

#if 0
  // if query executed
  FXVerticalFrame *queryTableFrame = new FXVerticalFrame(splitter, FRAME_SUNKEN | FRAME_THICK |
      LAYOUT_FILL_X | LAYOUT_FILL_Y, 0,0, 0, 0, 0,0,0,0);
//  FXText *text2 = new FXText(splitter, nullptr, 0, LAYOUT_FILL_X | LAYOUT_FILL_Y);
  FXTable *table=new FXTable(queryTableFrame,nullptr,0,TABLE_COL_SIZABLE|TABLE_ROW_SIZABLE|LAYOUT_FILL_X|LAYOUT_FILL_Y,0,0,0,0, 2,2,2,2);

  table->setVisibleRows(20);
  table->setVisibleColumns(8);

  // Remove row header
  table->setRowHeaderMode(LAYOUT_FIX_WIDTH);
  table->setRowHeaderWidth(0);

  table->setTableSize(50,14);
  table->setBackColor(FXRGB(255,255,255));
  table->setCellColor(0,0,FXRGB(255,255,255));
  table->setCellColor(0,1,FXRGB(255,240, 240));
  table->setCellColor(1,0,FXRGB(240, 255, 240));
  table->setCellColor(1,1,FXRGB(240, 240, 255));
  table->setHelpText("Editable table.");

//  table->setRowRenumbering(FXHeader::decimalNumbering);
//  table->setColumnRenumbering(FXHeader::alphaNumbering);

  int r, c;
  // Initialize scrollable part of table
  for(r=0; r<50; r++){
    for(c=0; c<14; c++){
      table->setItemText(r,c,"r:"+FXStringVal(r)+" c:"+FXStringVal(c));
      }
    }

  // Initialize column headers
  for(c=0; c<12; c++){
    table->setColumnText(c, "col" + FXStringVal(c));
    }

  // Initialize row headers
#if 1
  for(r=0; r<50; r++){
    table->setRowText(r,FXStringVal(r));
    }
#endif
#endif
}

void QueryTabItem::create()
{
  FXTabItem::create();

  frame->create();
  /*
  frame->create();
  splitter->create();
  queryTextFrame->create();
  text->create();
  queryTableFrame->create();
  table->create();
*/
}
