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

#include <algorithm> // std::replace
#include <cstring>
#include <stdexcept>
#include <langinfo.h>

// FreeTDS stuff

#include "SqlConnection.h"
#include "freetds/convert.h"

namespace tds {

static void (*g_log_func)(int level, const char *msg) = nullptr;

// Sadly, FreeTDS does not seem to check the return value of this message
// handler.
extern "C" int
sql_db_msg_handler(const TDSCONTEXT *context, TDSSOCKET *tds, const TDSMESSAGE *msg)
{
  if (msg->msgno == 5701 || msg->msgno == 5703 || msg->msgno == 5704) {
    return 0;
  }

  auto *conn = static_cast<SqlConnection *>(context->parent);
  return conn->MsgHandler(tds, msg->msgno, msg->state, msg->severity,
  msg->message, msg->server, msg->proc_name, msg->line_number);
}

int SqlConnection::MsgHandler(TDSSOCKET *socket, int msgno, int msgstate,
    int severity, char *msgtext, char *srvname, char *procname, int line)
{
  /*
   * If the severity is something other than 0 or the msg number is
   * 0 (user informational messages).
   */
  if (severity >= 0 || msgno == 0) {
    /*
     * If the message was something other than informational, and
     * the severity was greater than 0, then print information to
     * stderr with a little pre-amble information.
     */
    if (msgno > 0 && severity > 0) {
      _error.clear();

      // Incorporate format?
      _error += "Msg ";
      _error += std::to_string(msgno);
      _error += ", Level ";
      _error += std::to_string(severity);
      _error += ", State ";
      _error += std::to_string(msgstate);
      _error += "\nServer '";
      _error += srvname;
      _error += "'";

      if (procname != nullptr && *procname != '\0') {
        _error += ", Procedure '";
        _error += procname;
        _error += "'";
      }
      if (line > 0) {
        _error += ", Line ";
        _error += std::to_string(line);
      }
      _error += "\n";
      const char *database = socket->conn[0].env.database;
      if (database != nullptr) {
        _error += "Database '";
        _error += database;
        _error += "'\n";
      }
      _error += msgtext;
    } else {

      if (_error.back() != '\n') {
        _error.append(1, '\n');
      }

      _error += msgtext;
      if (msgno == 3621) {
        severity = 1;
      } else {
        severity = 0;
      }
    }
  }

  if (msgno == 904) {
    /* Database cannot be autostarted during server shutdown or startup */
    _error += "Database does not exist, returning 0.\n";
    return 0;
  }

  if (msgno == 911) {
    /* Database does not exist */
    _error += "Database does not exist, returning 0.\n";
    return 0;
  }

  if (msgno == 952) {
    /* Database is in transition. */
    _error += "Database is in transition, returning 0.\n";
    return 0;
  }

  tgt->handle(this, FXSEL(SEL_COMMAND, ID_ERROR), &_error);

  return severity > 0;
}

extern "C" int
sql_db_err_handler(const TDSCONTEXT *context, TDSSOCKET *tds, const TDSMESSAGE *msg)
{
  fprintf(stderr, "Error %d (severity %d):\n\t%s\n", msg->msgno, msg->severity,
          msg->message);
  if (msg->oserr != 0) {
    fprintf(stderr, "\tOS error %d, \"%s\"\n", msg->oserr, strerror(msg->oserr));
  }
  // For server messages, cancel the query and rely on the
  // message handler to capture the appropriate error message.
  return TDS_INT_CANCEL;
}

void sql_log(int level, const char *msg)
{
  if (g_log_func != nullptr) {
    g_log_func(level, msg);
  }
}

// FreeTDS DBLib requires some initialization.
void sql_startup(void (*log_func)(int, const char *))
{
  g_log_func = log_func;
}

SqlConnection::SqlConnection(const Server& serverInfo) :
    _serverInfo{serverInfo}
{
  context = tds_alloc_context(this);
  if (context == nullptr) {
    fprintf(stderr, "context cannot be null\n");
    return;
  }

  if (context->locale && !context->locale->date_fmt) {
    context->locale->date_fmt = strdup(STD_DATETIME_FMT);
  }
  context->msg_handler = sql_db_msg_handler;
  context->err_handler = sql_db_err_handler;
}

SqlConnection::~SqlConnection()
{
  Disconnect();
}


bool SqlConnection::Connect()
{
  // This can probably be massively simplified, as there is two login structs
  // being allocated.
  TDSLOGIN *login = tds_alloc_login();
  if (!login) {
    fprintf(stderr, "login cannot be null\n");
    return false;
  }

  // Populate Login properties
  tds_set_user(login, _serverInfo.user.text());
  tds_set_app(login, "TSQL");
  tds_set_library(login, "TDS-Library");
  tds_set_language(login, "us_english");
  tds_set_passwd(login, _serverInfo.password.text());
  tds_set_server(login, _serverInfo.server.text());
  tds_set_port(login, _serverInfo.port);

  _tds = tds_alloc_socket(context, 512);
  _tds->parent = nullptr;

  TDSLOGIN *connection = tds_read_config_info(_tds, login, context->locale);
  if (!connection)
    return false;

  // Get existing locale
  char *locale = setlocale(LC_ALL, nullptr);
  const char *charset = nl_langinfo(CODESET);

  if (locale) {
    printf("locale is %s\n", locale);
  }
  if (charset) {
    printf("locale charset is %s\n", charset);
  }

  if (tds_dstr_isempty(&connection->client_charset)) {
    if (!charset) {
      charset = "ISO-8859-1";
    }

    if (!tds_set_client_charset(login, charset)) {
      return false;
    }
    if (!tds_dstr_dup(&connection->client_charset, &login->client_charset)) {
      return false;
    }
  }

  printf("using default charset \"%s\"\n", tds_dstr_cstr(&connection->client_charset));

  if (!_serverInfo.default_database.empty()) {
    if (!tds_dstr_copy(&connection->database, _serverInfo.default_database.text())) {
      return false;
    }
    fprintf(stderr, "Setting %s as default database in login packet\n", _serverInfo.default_database.text());
  }

  if (TDS_FAILED(tds_connect_and_login(_tds, connection))) {
    tds_free_socket(_tds);
    tds_free_login(login);
    tds_free_context(context);
    fprintf(stderr, "There was a problem connecting to the server\n");
    return false;
  }

  if (!tds_dstr_isempty(&connection->instance_name)) {
    printf("Instance: %s on port %d", tds_dstr_cstr(&connection->instance_name), connection->port);
  }
  tds_free_login(connection);

  return true;
}
#if 0
void SqlConnection::run_initial_query()
{
  // Heterogeneous queries require the ANSI_NULLS and ANSI_WARNINGS options to
  // be set for the connection.
  ExecDML("SET ANSI_NULLS ON;"
      "SET ANSI_NULL_DFLT_ON ON;"
      "SET ANSI_PADDING ON;"
      "SET ANSI_WARNINGS ON;"
      "SET QUOTED_IDENTIFIER ON;"
      "SET CONCAT_NULL_YIELDS_NULL ON;");
}

#endif
void SqlConnection::Disconnect()
{
  if (_tds != nullptr) {
    tds_close_socket(_tds);
    tds_free_socket(_tds);
    _tds = nullptr;
  }
}

bool SqlConnection::SubmitQuery(const char *sql)
{
  TDSRET ret = tds_submit_query(_tds, sql);
  if (TDS_FAILED(ret)) {
    // Message raised to error handler?
    return false;
  }

  return true;
}

void SqlConnection::ProcessResults() {
  TDSRET rc;
  int i;
  int ctype;
  TDS_INT resulttype;
  TDS_INT srclen;
  unsigned char *src;
  TDSCOLUMN *col;
  int rows = 0;
  CONV_RESULT dres;
  static const char *opt_col_term = "\t";
  static const char *opt_row_term = "\n";

  while ((rc = tds_process_tokens(_tds, &resulttype, nullptr, TDS_TOKEN_RESULTS)) == TDS_SUCCESS) {
    const int stop_mask = TDS_STOPAT_ROWFMT | TDS_RETURN_DONE | TDS_RETURN_ROW | TDS_RETURN_COMPUTE;
#if 0
    if (opt_flags & OPT_TIMER) {
      gettimeofday(&start, NULL);
      print_rows = 0;
    }
#endif
    switch (resulttype) {
      case TDS_ROWFMT_RESULT:
        if (_tds->current_results != nullptr) {
          tgt->handle(this, FXSEL(SEL_COMMAND, ID_ROW_HEADER), _tds->current_results);
          for (i = 0; i < _tds->current_results->num_cols; i++) {
            if (i) fputs(opt_col_term, stdout);
            fputs(tds_dstr_cstr(&_tds->current_results->columns[i]->column_name), stdout);
          }
          fputs(opt_row_term, stdout);
        }
        break;
      case TDS_COMPUTE_RESULT:
      case TDS_ROW_RESULT:
        rows = 0;
        while ((rc = tds_process_tokens(_tds, &resulttype, nullptr, stop_mask)) == TDS_SUCCESS) {
          if (resulttype != TDS_ROW_RESULT && resulttype != TDS_COMPUTE_RESULT)
            break;

          rows++;

          if (!_tds->current_results)
            continue;

          for (i = 0; i < _tds->current_results->num_cols; i++) {
            col = _tds->current_results->columns[i];
            if (col->column_cur_size < 0) {
              if (i) fputs(opt_col_term, stdout);
              fputs("NULL", stdout);
              continue;
            }
            ctype = tds_get_conversion_type(col->column_type, col->column_size);

            src = col->column_data;
            if (is_blob_col(col) && col->column_type != SYBVARIANT) {
              src = (unsigned char *) ((TDSBLOB *) src)->textvalue;
            }
            srclen = col->column_cur_size;

            if (tds_convert(tds_get_ctx(_tds), ctype, src, srclen, SYBVARCHAR, &dres) < 0)
              continue;
            if (i) fputs(opt_col_term, stdout);
            fputs(dres.c, stdout);
            free(dres.c);
          }
          fputs(opt_row_term, stdout);
        }

//        if (!QUIET) printf("(%d row%s affected)\n", rows, rows == 1 ? "" : "s");
        break;
      case TDS_STATUS_RESULT:
#if 0
        if (!QUIET)
          printf("(return status = %d)\n", tds->ret_status);
#endif
        break;
      default:
        break;
    }
  }
}

#if 0

void SqlConnection::Dispose()
{
  // We're done, so clear our error state.
  _error.clear();

  // Did we already fetch all result sets?
  if (_fetched_results)
    return;

  // If the current result set has rows, drain them.
  if (!_fetched_rows) {
    while (NextRow());
  }

  // While there are more results, drain those rows as well.
  while (NextResult()) {
    while (NextRow());
  }
  _fetched_results = true;
}

void SqlConnection::ExecDML(const char *sql)
{
  Connect();

  // Dispose of any previous result set (if any).
  Dispose();

  _fetched_rows = false;
  _fetched_results = false;
  if (dbcmd(_dbHandle, sql) == FAIL)
    throw std::runtime_error("Failed to submit command to freetds");

  if (dbsqlexec(_dbHandle) == FAIL)
    throw std::runtime_error("Failed to execute DML");

  Dispose();
}


bool SqlConnection::NextResult()
{
  // In order to advance to the next result set, we need to fetch
  // all rows.
  if (!_fetched_rows) {
    while (NextRow());
  }

  int res = dbresults(_dbHandle);
  if (res == FAIL)
    throw std::runtime_error("Failed to fetch next result");

  if (res == NO_MORE_RESULTS) {
    _fetched_results = true;
    return false;
  }

  return true;
}

bool SqlConnection::NextRow()
{
  int row_code;
  do {
    row_code = dbnextrow(_dbHandle);
    if (row_code == NO_MORE_ROWS) {
      _fetched_rows = true;
      return false;
    }

    if (row_code == REG_ROW)
      return true;

    if (row_code == FAIL) {
      // ERROR ??
      return false;
    }
  } while (row_code == BUF_FULL);

  return false;
}

void SqlConnection::ExecSql(const char *sql)
{
  Connect();

  // Dispose of any previous result set (if any).
  Dispose();

  _fetched_rows = false;
  _fetched_results = false;
  if (dbcmd(_dbHandle, sql) == FAIL)
    throw std::runtime_error("Failed to submit command to freetds");

  if (dbsqlexec(_dbHandle) == FAIL) {
    if (!_error.empty()) {
      throw std::runtime_error(_error);
    } else {
      throw std::runtime_error("Failed to execute SQL");
    }
  }

  int res = dbresults(_dbHandle);
  if (res == FAIL)
    throw std::runtime_error("Failed to fetch results SQL");

  if (res == NO_MORE_RESULTS)
    _fetched_results = true;
}

int
SqlConnection::GetOrdinal(const char *colName)
{
  int i;
  char errorStr[2048];
  int total_cols = dbnumcols(_dbHandle);

  for (i = 0; i < total_cols; i++) {
    if (strcmp(dbcolname(_dbHandle, i + 1), colName) == 0) {
      break;
    }
  }
  if (i == total_cols) {
    snprintf(errorStr, sizeof(errorStr),
        "Requested column '%s' but does not exist.", colName);
    throw std::runtime_error(errorStr);
  }

  return i;
}

std::string
SqlConnection::GetStringCol(int col)
{
  if (col > dbnumcols(_dbHandle))
    throw std::runtime_error("Requested string on nonexistent column");

  int coltype = dbcoltype(_dbHandle, col + 1);
  DBINT srclen = dbdatlen(_dbHandle, col + 1);

  if (coltype == SYBDATETIME) {
    DBDATETIME data;
    DBDATEREC output;
    char date_string[64];

    memcpy(&data, dbdata(_dbHandle, col + 1), srclen);
    dbdatecrack(_dbHandle, &output, &data);
    snprintf(date_string, sizeof(date_string),
        "%04d-%02d-%02d %02d:%02d:%02d.%03d", output.year, output.month,
        output.day, output.hour, output.minute, output.second,
        output.millisecond);

    return date_string;
  } else if (coltype != SYBCHAR && coltype != SYBTEXT) {
    char nonstr[4096];
    int dest_size;

    dest_size = dbconvert(_dbHandle, coltype, dbdata(_dbHandle, col + 1),
        srclen, SYBCHAR, (BYTE *)nonstr, sizeof(nonstr));
    if (dest_size == -1) {
      throw std::runtime_error("Could not convert source to string.");
    }
    nonstr[dest_size] = '\0';
    return nonstr;
  }
  return std::string((const char *)dbdata(_dbHandle, col + 1), srclen);
}

std::string
SqlConnection::GetStringColByName(const char *colName)
{
  return GetStringCol(GetOrdinal(colName));
}

int
SqlConnection::GetInt32ColByName(const char *colName)
{
  return GetInt32Col(GetOrdinal(colName));
}

int
SqlConnection::GetInt32Col(int col)
{
  int ret;
  DBINT srclen;
  int coltype;

  if (col > dbnumcols(_dbHandle))
    return 0;

  srclen = dbdatlen(_dbHandle, col + 1);
  coltype = dbcoltype(_dbHandle, col + 1);

  if (coltype == SYBINT4) {
    memcpy(&ret, dbdata(_dbHandle, col + 1), srclen);
  } else if (coltype == SYBNUMERIC) {
    dbconvert(nullptr, SYBNUMERIC, (BYTE *)dbdata(_dbHandle, col + 1), -1,
      SYBINT4, (BYTE *)&ret, 4);
  } else {
    dbconvert(nullptr, coltype, (BYTE *)dbdata(_dbHandle, col + 1), -1,
      SYBINT4, (BYTE *)&ret, 4);
  }

  return ret;
}

int
SqlConnection::GetMoneyCol(int col, int *dol, int *cen)
{
  int64_t t64;
  BYTE *src;

  if (col > dbnumcols(_dbHandle))
    return 0;

  src = dbdata(_dbHandle, col + 1);
  t64 = (int64_t)src[4] | (int64_t)src[5] << 8 |
      (int64_t)src[6] << 16 | (int64_t)src[7] << 24 |
      (int64_t)src[0] << 32 | (int64_t)src[1] << 40 |
      (int64_t)src[2] << 48 | (int64_t)src[3] << 56;

  *dol = (int)(t64 / 10000);
  if (t64 < 0) t64 = -t64;
  *cen = (int)(t64 % 10000);

  return 1;
}

bool
SqlConnection::IsNullCol(int col)
{
  if (col > dbnumcols(_dbHandle))
    return true;

  DBINT srclen = dbdatlen(_dbHandle, col + 1);
  return srclen <= 0;
}

std::string SqlConnection::fix_server(const char *str)
{
  std::string clean_server = str;

  // A server contains a host and port, but users may be providing
  // a server string with properties that aren't supported by FreeTDS.

  // FreeTDS doesn't need the leading "tcp:" in some connection strings.
  // Remove it if it exists.
  if (std::string tcp_prefix("tcp:"); clean_server.compare(0, tcp_prefix.size(), tcp_prefix) == 0)
    clean_server = clean_server.substr(tcp_prefix.size());

  // Some people use commas instead of colon to separate the port number.
  std::replace(clean_server.begin(), clean_server.end(), ',', ':');

  return clean_server;
}

std::vector<std::string> SqlConnection::GetAllColumnNames()
{
  int total_cols = dbnumcols(_dbHandle);

  std::vector<std::string> columns;
  columns.reserve(total_cols);

  for (int i = 0; i < total_cols; i++) {
    columns.emplace_back(dbcolname(_dbHandle, i + 1));
  }

  return columns;
}
#endif

}
