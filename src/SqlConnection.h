/*
 * Copyright (c) 2012-2021 Devin Smith <devin@devinsmith.net>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef TDS_SQLCONNECTION_H
#define TDS_SQLCONNECTION_H

#include <string>
#include <vector>
#include <fx.h>

#include "tds/include/freetds/tds.h"

#include "Server.h"

namespace tds {

class SqlConnection : public FXObject {
public:
  explicit SqlConnection(const Server& serverInfo);

  ~SqlConnection();

  enum {
    ID_READ = FXMainWindow::ID_LAST+1100,
    ID_ROW_HEADER,
    ID_ROW_READ,
    ID_ERROR,
  };

  // No move or copy support. I don't want to deal with various pointers.
  SqlConnection(const SqlConnection&) = delete;
  SqlConnection& operator=(const SqlConnection&) = delete;
  SqlConnection(SqlConnection&&) = delete;
  SqlConnection& operator=(SqlConnection&&) = delete;

  void setTarget(FXObject *target) { tgt = target; }

  // Executing a stored procedure or query will automatically connect
  // It should not be necessary to call this method directly.
  bool Connect();

  void Disconnect();

  bool SubmitQuery(const char *sql);
  void ProcessResults();
#if 0

  // When a query is executed freetds buffers the results into a
  // local buffer. Dispose must be called to clear out the results before
  // another query is run.
  void Dispose();

  // Execute Data Manipulation Language (UPDATE/INSERT/DELETE/etc)
  void ExecDML(const char *sql);

  // Execute SQL where you expect results/resultsets (SELECT).
  void ExecSql(const char *sql);

  // Move to next result set.
  bool NextResult();

  // Move to the next row.
  bool NextRow();

  // Data extraction
  int GetOrdinal(const char *colName);

  std::vector<std::string> GetAllColumnNames();

  std::string GetStringCol(int col);
  std::string GetStringColByName(const char *colName);
  int GetInt32Col(int col);
  int GetInt32ColByName(const char *colName);
  int GetMoneyCol(int col, int *dol_out, int *cen_out);
  bool IsNullCol(int col);
#endif
  // FreeTDS callback helper
  int MsgHandler(TDSSOCKET *socket, int msgno, int msgstate,
      int severity, char *msgtext, char *srvname, char *procname, int line);

private:
#if 0
  void run_initial_query();
  static std::string fix_server(const char *str);
#endif
  const Server& _serverInfo;
  FXObject *tgt{nullptr};
  TDSCONTEXT *context{nullptr};
  TDSSOCKET *_tds{nullptr};
  std::string _error;

};

void sql_startup(void (*log_func)(int, const char *));
void sql_log(int level, const char *msg);

} // namespace tds

#endif // TDS_SQLCONNECTION_H
