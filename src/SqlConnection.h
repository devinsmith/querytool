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

#include <sqlfront.h>
#include <sybdb.h>

#include "Server.h"

namespace tds {

class SqlConnection {
public:
  explicit SqlConnection(const Server& serverInfo) :
    _serverInfo{serverInfo}, _dbHandle{nullptr},
    _fetched_rows{true}, _fetched_results{true} {}

  ~SqlConnection();

  // No move or copy support. I don't want to deal with DBPROCESS pointer.
  SqlConnection(const SqlConnection&) = delete;
  SqlConnection& operator=(const SqlConnection&) = delete;
  SqlConnection(SqlConnection&&) = delete;
  SqlConnection& operator=(SqlConnection&&) = delete;

  // Executing a stored procedure or query will automatically connect
  // It should not be necessary to call this method directly.
  bool Connect();

  void Disconnect();

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

  // FreeTDS callback helper
  int MsgHandler(DBPROCESS * dbproc, DBINT msgno, int msgstate,
    int severity, char *msgtext, char *srvname, char *procname, int line);

private:
  void run_initial_query();
  static std::string fix_server(const char *str);

  const Server& _serverInfo;
  DBPROCESS *_dbHandle;
  bool _fetched_rows;
  bool _fetched_results;
  std::string _error;
};

void sql_startup(void (*log_func)(int, const char *));
void sql_shutdown();
void sql_log(int level, const char *msg);

} // namespace tds

#endif // TDS_SQLCONNECTION_H
