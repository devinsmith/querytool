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
#include "freetds/convert.h"

FXDEFMAP(QueryTabItem) queryTabItemMap[] = {
  FXMAPFUNC(SEL_COMMAND, tds::SqlConnection::ID_ROW_HEADER, QueryTabItem::OnRowHeaderRead),
  FXMAPFUNC(SEL_COMMAND, tds::SqlConnection::ID_ROW_READ, QueryTabItem::OnRowRead)
};

FXIMPLEMENT(QueryTabItem, FXTabItem, queryTabItemMap, ARRAYNUMBER(queryTabItemMap))

QueryTabItem::QueryTabItem(FXTabBook *tabbook, const FXString& label, tds::SqlConnection *conn) :
  FXTabItem(tabbook, label, nullptr), parent(tabbook), conn{conn}
{
  frame = new FXVerticalFrame(parent, FRAME_THICK|FRAME_RAISED);

  splitter = new FXSplitter(frame, SPLITTER_VERTICAL | SPLITTER_REVERSED | LAYOUT_FILL_X | LAYOUT_FILL_Y);
  FXVerticalFrame *queryTextFrame = new FXVerticalFrame(splitter, FRAME_SUNKEN | FRAME_THICK |
      LAYOUT_FILL_X | LAYOUT_FILL_Y, 0,0, 0, 0, 0,0,0,0);
  text = new FXText(queryTextFrame, nullptr, 0, LAYOUT_FILL_X | LAYOUT_FILL_Y);

  queryFrame = new FXVerticalFrame(splitter, FRAME_SUNKEN | FRAME_THICK |
                                             LAYOUT_FILL_X | LAYOUT_FILL_Y, 0, 0, 0, 0, 0, 0, 0, 0);
  queryFrame->hide();


  statusBar = new FXStatusBar(frame, LAYOUT_FILL_X);

  conn->setTarget(this);
#if 0
  // if query executed
  queryFrame = new FXVerticalFrame(splitter, FRAME_SUNKEN | FRAME_THICK |
      LAYOUT_FILL_X | LAYOUT_FILL_Y, 0,0, 0, 0, 0,0,0,0);

  queryFrame->setHeight(0);
  splitter->hide();
  //queryFrame->hide();

#endif
#if 0
//  FXText *text2 = new FXText(splitter, nullptr, 0, LAYOUT_FILL_X | LAYOUT_FILL_Y);
  FXTable *table=new FXTable(queryFrame,nullptr,0,TABLE_COL_SIZABLE|TABLE_ROW_SIZABLE|LAYOUT_FILL_X|LAYOUT_FILL_Y,0,0,0,0, 2,2,2,2);

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

void QueryTabItem::ExecuteQuery()
{
  //
  printf("Executing %s\n", text->getText().text());

#if 0
  if (queryFrame == nullptr) {
    queryFrame = new FXVerticalFrame(splitter, FRAME_SUNKEN | FRAME_THICK |
                                               LAYOUT_FILL_X | LAYOUT_FILL_Y, 0, 0, 0, 0, 0, 0, 0, 0);
    queryFrame->create();
    queryFrame->show();
    int numChildren = queryFrame->numChildren();

    printf("NEW: Num children: %d\n", numChildren);

  } else {
#endif
    int numChildren = queryFrame->numChildren();
    printf("OLD: Num children: %d\n", numChildren);
    for (int i = 0; i < numChildren; i++) {
      FXWindow *child = queryFrame->childAtIndex(i);
      child->destroy();
      delete child;
    }
#if 0
  }
#endif
  queryFrame->show();



  statusBar->getStatusLine()->setNormalText("Executing query");



  // submit to freetds
  conn->SubmitQuery(text->getText().text());
  conn->ProcessResults();

  printf("After process results\n");

  statusBar->getStatusLine()->setNormalText("Done!");

#if 0
  if (resultTable != nullptr) {
    resultTable->destroy();
    delete resultTable;
  }
#endif

#if 0

  resultTable = new FXTable(queryFrame,nullptr,0,TABLE_COL_SIZABLE|TABLE_ROW_SIZABLE|LAYOUT_FILL_X|LAYOUT_FILL_Y,0,0,0,0, 2,2,2,2);
  resultTable->setRowHeaderMode(LAYOUT_FIX_WIDTH);
  //resultTable->setRowHeaderWidth(0);

  resultTable->create();
  resultTable->show();
#endif
#if 0
  FXText *text2 = new FXText(queryFrame, nullptr, 0, LAYOUT_FILL_X | LAYOUT_FILL_Y);
  text2->create();
  text2->show();

  //queryFrame->layout();
#endif



}

long QueryTabItem::OnRowHeaderRead(FX::FXObject *, FX::FXSelector, void *data)
{
  printf("%s\n", __func__ );

  auto *resultInfo = static_cast<TDSRESULTINFO *>(data);

  resultTable = new FXTable(queryFrame,nullptr,0,TABLE_COL_SIZABLE|TABLE_ROW_SIZABLE|LAYOUT_FILL_X|LAYOUT_FILL_Y,0,0,0,0, 2,2,2,2);
  resultTable->setRowHeaderMode(LAYOUT_FIX_WIDTH);
  resultTable->setRowHeaderWidth(0);
  resultTable->setTableSize(0,resultInfo->num_cols);

  resultTable->setBackColor(FXRGB(255,255,255));
  resultTable->setCellColor(0,0,FXRGB(255,255,255));
  resultTable->setCellColor(0,1,FXRGB(255,240, 240));
  resultTable->setCellColor(1,0,FXRGB(240, 255, 240));
  resultTable->setCellColor(1,1,FXRGB(240, 240, 255));

  for(int c=0; c<resultInfo->num_cols; c++){
    resultTable->setColumnText(c, tds_dstr_cstr(&resultInfo->columns[c]->column_name));
  }

  resultTable->create();
  resultTable->show();




  queryFrame->layout();
  queryFrame->recalc();
  queryFrame->update();

#if 0
  resultTable = new FXTable(queryFrame,nullptr,0,TABLE_COL_SIZABLE|TABLE_ROW_SIZABLE|LAYOUT_FILL_X|LAYOUT_FILL_Y,0,0,0,0, 2,2,2,2);

  resultTable->setVisibleRows(20);
  resultTable->setVisibleColumns(resultInfo->num_cols);

  // Remove row header
  resultTable->setRowHeaderMode(LAYOUT_FIX_WIDTH);
  resultTable->setRowHeaderWidth(0);
  resultTable->setTableSize(50,resultInfo->num_cols);

  for(int c=0; c<resultInfo->num_cols; c++){
    resultTable->setColumnText(c, tds_dstr_cstr(&resultInfo->columns[c]->column_name));
  }

  resultTable->create();
  resultTable->show();
#endif
  return 1;
}

long QueryTabItem::OnRowRead(FX::FXObject *, FX::FXSelector, void *data)
{
  printf("%s\n", __PRETTY_FUNCTION__);
  auto *resultInfo = static_cast<TDSRESULTINFO *>(data);

  //int col = resultTable->getNumColumns();
  int row = resultTable->getNumRows();
  printf("Num row: %d\n", row);

  resultTable->insertRows(row);
  //row++;

  for(int c=0; c<resultInfo->num_cols; c++){

    auto *col = resultInfo->columns[c];
    if (col->column_cur_size < 0) {
      resultTable->setItemText(row,c,"NULL");
      continue;
    }

    int ctype = tds_get_conversion_type(col->column_type, col->column_size);

    unsigned char *src = col->column_data;
    if (is_blob_col(col) && col->column_type != SYBVARIANT) {
      src = (unsigned char *) ((TDSBLOB *) src)->textvalue;
    }
    int srclen = col->column_cur_size;

    CONV_RESULT dres;
    if (tds_convert(conn->getContext(), ctype, src, srclen, SYBVARCHAR, &dres) < 0)
      continue;
    resultTable->setItemText(row,c,dres.c);
    free(dres.c);
  }

  // setTableSize will clear out the columns.
  //resultTable->setTableSize(row + 1, col, TRUE);

}

void QueryTabItem::create()
{
  FXTabItem::create();

  frame->create();
  /*
  splitter->create();
//  queryTextFrame->create();
  text->create();
//  queryTableFrame->create();
//  table->create();
   */
}
