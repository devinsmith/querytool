/* FreeTDS - Library of routines accessing Sybase and Microsoft databases
 * Copyright (C) 1998, 1999, 2000, 2001, 2002, 2003, 2004, 2005  Brian Bruns
 * Copyright (C) 2006, 2007, 2008, 2009, 2010, 2011  Frediano Ziglio
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <config.h>

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ctype.h>

#include <freetds/tds.h>
#include <freetds/iconv.h>
#include <freetds/utils/string.h>

#include <assert.h>

static void tds7_put_query_params(TDSSOCKET * tds, const char *query, size_t query_len);
static TDSRET tds_put_data_info(TDSSOCKET * tds, TDSCOLUMN * curcol, int flags);
static inline TDSRET tds_put_data(TDSSOCKET * tds, TDSCOLUMN * curcol);
static TDSRET tds7_write_param_def_from_query(TDSSOCKET * tds, const char* converted_query,
					      size_t converted_query_len, TDSPARAMINFO * params);
static TDSRET tds7_write_param_def_from_params(TDSSOCKET * tds, const char* query, size_t query_len,
					       TDSPARAMINFO * params);

static int tds_count_placeholders_ucs2le(const char *query, const char *query_end);

#define TDS_PUT_DATA_USE_NAME 1
#define TDS_PUT_DATA_PREFIX_NAME 2
#define TDS_PUT_DATA_LONG_STATUS 4

#undef MIN
#define MIN(a,b) (((a) < (b)) ? (a) : (b))
#undef MAX
#define MAX(a,b) (((a) > (b)) ? (a) : (b))

/* All manner of client to server submittal functions */

/**
 * \ingroup libtds
 * \defgroup query Query
 * Function to handle query.
 */

/**
 * \addtogroup query
 * @{ 
 */

/**
 * Accept an ASCII string, convert it to UCS2-LE
 * The input is NUL-terminated, but the output does not contains the NUL.
 * \param buffer buffer where to store output
 * \param buf string to write
 * \return bytes written
 */
static size_t
tds_ascii_to_ucs2(char *buffer, const char *buf)
{
	char *s;
	assert(buffer && buf && *buf); /* This is an internal function.  Call it correctly. */

	for (s = buffer; *buf != '\0'; ++buf) {
		*s++ = *buf;
		*s++ = '\0';
	}

	return s - buffer;
}

/**
 * Utility to convert a constant ascii string to ucs2 and send to server.
 * Used to send internal store procedure names to server.
 * \tds
 * \param s  constanst string to send
 */
#define TDS_PUT_N_AS_UCS2(tds, s) do { \
	char buffer[sizeof(s)*2-2]; \
	tds_put_smallint(tds, sizeof(buffer)/2); \
	tds_put_n(tds, buffer, tds_ascii_to_ucs2(buffer, s)); \
} while(0)

/**
 * Convert a string in an allocated buffer
 * \param tds        state information for the socket and the TDS protocol
 * \param char_conv  information about the encodings involved
 * \param s          input string
 * \param len        input string length (in bytes), -1 for NUL-terminated
 * \param out_len    returned output length (in bytes)
 * \return string allocated (or input pointer if no conversion required) or NULL if error
 */
const char *
tds_convert_string(TDSSOCKET * tds, TDSICONV * char_conv, const char *s, int len, size_t *out_len)
{
	char *buf;

	const char *ib;
	char *ob;
	size_t il, ol;

	/* char_conv is only mostly const */
	TDS_ERRNO_MESSAGE_FLAGS *suppress = (TDS_ERRNO_MESSAGE_FLAGS*) &char_conv->suppress;

	il = len < 0 ? strlen(s) : (size_t) len;
	if (char_conv->flags == TDS_ENCODING_MEMCPY) {
		*out_len = il;
		return s;
	}

	/* allocate needed buffer (+1 is to exclude 0 case) */
	ol = il * char_conv->to.charset.max_bytes_per_char / char_conv->from.charset.min_bytes_per_char + 1;
	buf = tds_new(char, ol);
	if (!buf)
		return NULL;

	ib = s;
	ob = buf;
	memset(suppress, 0, sizeof(char_conv->suppress));
	if (tds_iconv(tds, char_conv, to_server, &ib, &il, &ob, &ol) == (size_t)-1) {
		free(buf);
		return NULL;
	}
	*out_len = ob - buf;
	return buf;
}

#if ENABLE_EXTRA_CHECKS
void
tds_convert_string_free(const char *original, const char *converted)
{
	if (original != converted)
		free((char *) converted);
}
#endif

/**
 * Flush query packet.
 * Used at the end of packet write to really send packet to server.
 * This also changes the state to TDS_PENDING.
 * \tds
 */
static TDSRET
tds_query_flush_packet(TDSSOCKET *tds)
{
	TDSRET ret = tds_flush_packet(tds);
	/* TODO depend on result ?? */
	tds_set_state(tds, TDS_PENDING);
	return ret;
}

/**
 * Set current dynamic.
 * \tds
 * \param dyn  dynamic to set
 */
void
tds_set_cur_dyn(TDSSOCKET *tds, TDSDYNAMIC *dyn)
{
	if (dyn)
		++dyn->ref_count;
	tds_release_cur_dyn(tds);
	tds->cur_dyn = dyn;
}

/**
 * Sends a language string to the database server for
 * processing.  TDS 4.2 is a plain text message with a packet type of 0x01,
 * TDS 7.0 is a unicode string with packet type 0x01, and TDS 5.0 uses a 
 * TDS_LANGUAGE_TOKEN to encapsulate the query and a packet type of 0x0f.
 * \tds
 * \param query language query to submit
 * \return TDS_FAIL or TDS_SUCCESS
 */
TDSRET
tds_submit_query(TDSSOCKET * tds, const char *query)
{
	return tds_submit_query_params(tds, query, NULL, NULL);
}

/**
 * Write data to wire
 * \tds
 * \param curcol  column where store column information
 * \return TDS_FAIL on error or TDS_SUCCESS
 */
static inline TDSRET
tds_put_data(TDSSOCKET * tds, TDSCOLUMN * curcol)
{
	return curcol->funcs->put_data(tds, curcol, 0);
}

/**
 * Start query packet of a given type
 * \tds
 * \param packet_type  packet type
 * \param head         extra information to put in a TDS7 header
 */
static TDSRET
tds_start_query_head(TDSSOCKET *tds, unsigned char packet_type, TDSHEADERS * head)
{
	tds->out_flag = packet_type;
	if (IS_TDS72_PLUS(tds->conn)) {
		TDSFREEZE outer;

		tds_freeze(tds, &outer, 4);                    /* total length */
		tds_put_int(tds, 18);                          /* length: transaction descriptor */
		tds_put_smallint(tds, 2);                      /* type: transaction descriptor */
		tds_put_n(tds, tds->conn->tds72_transaction, 8);  /* transaction */
		tds_put_int(tds, 1);                           /* request count */
		if (head && head->qn_msgtext && head->qn_options) {
			TDSFREEZE query;

			tds_freeze(tds, &query, 4);                    /* length: query notification */
			tds_put_smallint(tds, 1);                      /* type: query notification */

			TDS_START_LEN_USMALLINT(tds) {
				tds_put_string(tds, head->qn_msgtext, -1);     /* notifyid */
			} TDS_END_LEN

			TDS_START_LEN_USMALLINT(tds) {
				tds_put_string(tds, head->qn_options, -1);     /* ssbdeployment */
			} TDS_END_LEN

			if (head->qn_timeout != 0)
				tds_put_int(tds, head->qn_timeout);    /* timeout */

			tds_freeze_close_len(&query, tds_freeze_written(&query));
		}
		tds_freeze_close_len(&outer, tds_freeze_written(&outer));
	}
	return TDS_SUCCESS;
}

/**
 * Start query packet of a given type
 * \tds
 * \param packet_type  packet type
 */
void
tds_start_query(TDSSOCKET *tds, unsigned char packet_type)
{
	/* no need to check return value here because tds_start_query_head() cannot
	fail when given a NULL head parameter */
	tds_start_query_head(tds, packet_type, NULL);
}

/**
 * Sends a language string to the database server for
 * processing.  TDS 4.2 is a plain text message with a packet type of 0x01,
 * TDS 7.0 is a unicode string with packet type 0x01, and TDS 5.0 uses a
 * TDS_LANGUAGE_TOKEN to encapsulate the query and a packet type of 0x0f.
 * \tds
 * \param query  language query to submit
 * \param params parameters of query
 * \return TDS_FAIL or TDS_SUCCESS
 */
TDSRET
tds_submit_query_params(TDSSOCKET * tds, const char *query, TDSPARAMINFO * params, TDSHEADERS * head)
{
	size_t query_len;
	int num_params = params ? params->num_cols : 0;

	if (!query)
		return TDS_FAIL;
 
	if (tds_set_state(tds, TDS_WRITING) != TDS_WRITING)
		return TDS_FAIL;
 
	query_len = strlen(query);
 
	if (!IS_TDS7_PLUS(tds->conn) || params == NULL || params->num_cols == 0) {
		if (tds_start_query_head(tds, TDS_QUERY, head) != TDS_SUCCESS)
			return TDS_FAIL;
		tds_put_string(tds, query, (int)query_len);
	} else {
		TDSCOLUMN *param;
		int count, i;
		size_t converted_query_len;
		const char *converted_query;
		TDSFREEZE outer;
		TDSRET rc;

		converted_query = tds_convert_string(tds, tds->conn->char_convs[client2ucs2], query, (int)query_len, &converted_query_len);
		if (!converted_query) {
			tds_set_state(tds, TDS_IDLE);
			return TDS_FAIL;
		}

		count = tds_count_placeholders_ucs2le(converted_query, converted_query + converted_query_len);

		if (tds_start_query_head(tds, TDS_RPC, head) != TDS_SUCCESS) {
			tds_convert_string_free(query, converted_query);
			return TDS_FAIL;
		}

		tds_freeze(tds, &outer, 0);

		/* procedure name */
		if (IS_TDS71_PLUS(tds->conn)) {
			tds_put_smallint(tds, -1);
			tds_put_smallint(tds, TDS_SP_EXECUTESQL);
		} else {
			TDS_PUT_N_AS_UCS2(tds, "sp_executesql");
		}
		tds_put_smallint(tds, 0);
 
		/* string with sql statement */
		if (!count) {
			tds_put_byte(tds, 0);
			tds_put_byte(tds, 0);
			tds_put_byte(tds, SYBNTEXT);	/* must be Ntype */
			TDS_PUT_INT(tds, converted_query_len);
			if (IS_TDS71_PLUS(tds->conn))
				tds_put_n(tds, tds->conn->collation, 5);
			TDS_PUT_INT(tds, converted_query_len);
			tds_put_n(tds, converted_query, converted_query_len);

			rc = tds7_write_param_def_from_params(tds, converted_query, converted_query_len, params);
		} else {
			tds7_put_query_params(tds, converted_query, converted_query_len);

			rc = tds7_write_param_def_from_query(tds, converted_query, converted_query_len, params);
		}
		tds_convert_string_free(query, converted_query);
		if (TDS_FAILED(rc)) {
			tds_freeze_abort(&outer);
			return rc;
		}
		tds_freeze_close(&outer);

		for (i = 0; i < num_params; i++) {
			param = params->columns[i];
			TDS_PROPAGATE(tds_put_data_info(tds, param, 0));
			TDS_PROPAGATE(tds_put_data(tds, param));
		}
		tds->current_op = TDS_OP_EXECUTESQL;
	}
	return tds_query_flush_packet(tds);
}

/**
 * Format and submit a query
 * \tds
 * \param queryf  query format. printf like expansion is performed on
 *        this query.
 */
TDSRET
tds_submit_queryf(TDSSOCKET * tds, const char *queryf, ...)
{
	va_list ap;
	char *query = NULL;
	TDSRET rc = TDS_FAIL;

	va_start(ap, queryf);
	if (vasprintf(&query, queryf, ap) >= 0) {
		rc = tds_submit_query(tds, query);
		free(query);
	}
	va_end(ap);
	return rc;
}

/**
 * Skip a comment in a query
 * \param s    start of the string (or part of it)
 * \returns pointer to end of comment
 */
const char *
tds_skip_comment(const char *s)
{
	const char *p = s;

	if (*p == '-' && p[1] == '-') {
		for (;*++p != '\0';)
			if (*p == '\n')
				return p + 1;
	} else if (*p == '/' && p[1] == '*') {
		++p;
		for(;*++p != '\0';)
			if (*p == '*' && p[1] == '/')
				return p + 2;
	} else
		++p;

	return p;
}

/**
 * Skip quoting string (like 'sfsf', "dflkdj" or [dfkjd])
 * \param s pointer to first quoting character. @verbatim Should be ', " or [. @endverbatim
 * \return character after quoting
 */
const char *
tds_skip_quoted(const char *s)
{
	const char *p = s;
	char quote = (*s == '[') ? ']' : *s;

	for (; *++p;) {
		if (*p == quote) {
			if (*++p != quote)
				return p;
		}
	}
	return p;
}

/**
 * Get position of next placeholder
 * \param start pointer to part of query to search
 * \return next placeholder or NULL if not found
 */
const char *
tds_next_placeholder(const char *start)
{
	const char *p = start;

	if (!p)
		return NULL;

	for (;;) {
		switch (*p) {
		case '\0':
			return NULL;
		case '\'':
		case '\"':
		case '[':
			p = tds_skip_quoted(p);
			break;

		case '-':
		case '/':
			p = tds_skip_comment(p);
			break;

		case '?':
			return p;
		default:
			++p;
			break;
		}
	}
}

/**
 * Count the number of placeholders ('?') in a query
 * \param query  query string
 */
int
tds_count_placeholders(const char *query)
{
	const char *p = query - 1;
	int count = 0;

	for (;; ++count) {
		if (!(p = tds_next_placeholder(p + 1)))
			return count;
	}
}

/**
 * Skip a comment in a query
 * \param s    start of the string (or part of it). Encoded in ucs2le
 * \param end  end of string
 * \returns pointer to end of comment
 */
static const char *
tds_skip_comment_ucs2le(const char *s, const char *end)
{
	const char *p = s;

	if (p+4 <= end && memcmp(p, "-\0-", 4) == 0) {
		for (;(p+=2) < end;)
			if (p[0] == '\n' && p[1] == 0)
				return p + 2;
	} else if (p+4 <= end && memcmp(p, "/\0*", 4) == 0) {
		p += 2;
		end -= 2;
		for(;(p+=2) < end;)
			if (memcmp(p, "*\0/", 4) == 0)
				return p + 4;
		return end + 2;
	} else
		p += 2;

	return p;
}


/**
 * Return pointer to end of a quoted string.
 * At the beginning pointer should point to delimiter.
 * \param s    start of string to skip encoded in ucs2le
 * \param end  pointer to end of string
 */
static const char *
tds_skip_quoted_ucs2le(const char *s, const char *end)
{
	const char *p = s;
	char quote = (*s == '[') ? ']' : *s;

	assert(s[1] == 0 && s < end && (end - s) % 2 == 0);

	for (; (p += 2) != end;) {
		if (p[0] == quote && !p[1]) {
			p += 2;
			if (p == end || p[0] != quote || p[1])
				return p;
		}
	}
	return p;
}

/**
 * Found the next placeholder (? or \@param) in a string.
 * String must be encoded in ucs2le.
 * \param start  start of the string (or part of it)
 * \param end    end of string
 * \param named  true if named parameters should be returned
 * \returns either start of next placeholder or end if not found
 */
static const char *
tds_next_placeholder_ucs2le(const char *start, const char *end, int named)
{
	const char *p = start;
	char prev = ' ', c;

	assert(p && start <= end && (end - start) % 2 == 0);

	for (; p != end;) {
		if (p[1]) {
			prev = ' ';
			p += 2;
			continue;
		}
		c = p[0];
		switch (c) {
		case '\'':
		case '\"':
		case '[':
			p = tds_skip_quoted_ucs2le(p, end);
			break;

		case '-':
		case '/':
			p = tds_skip_comment_ucs2le(p, end);
			c = ' ';
			break;

		case '?':
			return p;
		case '@':
			if (named && !isalnum((unsigned char) prev))
				return p;
		default:
			p += 2;
			break;
		}
		prev = c;
	}
	return end;
}

/**
 * Count the number of placeholders ('?') in a query
 * \param query      query encoded in ucs2le
 * \param query_end  end of query
 * \return number of placeholders found
 */
static int
tds_count_placeholders_ucs2le(const char *query, const char *query_end)
{
	const char *p = query - 2;
	int count = 0;

	for (;; ++count) {
		if ((p = tds_next_placeholder_ucs2le(p + 2, query_end, 0)) == query_end)
			return count;
	}
}

/**
 * Return declaration for column (like "varchar(20)").
 *
 * This depends on:
 * - on_server.column_type
 * - varint_size (for varchar(max) distinction)
 * - column_size
 * - precision/scale (numeric)
 *
 * \tds
 * \param curcol column
 * \param out    buffer to hold declaration
 * \return TDS_FAIL or TDS_SUCCESS
 */
TDSRET
tds_get_column_declaration(TDSSOCKET * tds, TDSCOLUMN * curcol, char *out)
{
	const char *fmt = NULL;
	/* unsigned int is required by printf format, don't use size_t */
	unsigned int max_len = IS_TDS7_PLUS(tds->conn) ? 8000 : 255;
	unsigned int size;

	size = tds_fix_column_size(tds, curcol);

	switch (tds_get_conversion_type(curcol->on_server.column_type, curcol->on_server.column_size)) {
	case XSYBCHAR:
	case SYBCHAR:
		fmt = "CHAR(%u)";
		break;
	case SYBVARCHAR:
	case XSYBVARCHAR:
		if (curcol->column_varint_size == 8)
			fmt = "VARCHAR(MAX)";
		else
			fmt = "VARCHAR(%u)";
		break;
	case SYBUINT1:
	case SYBINT1:
		fmt = "TINYINT";
		break;
	case SYBINT2:
		fmt = "SMALLINT";
		break;
	case SYBINT4:
		fmt = "INT";
		break;
	case SYBINT8:
		/* TODO even for Sybase ?? */
		fmt = "BIGINT";
		break;
	case SYBFLT8:
		fmt = "FLOAT";
		break;
	case SYBDATETIME:
		fmt = "DATETIME";
		break;
	case SYBDATE:
		fmt = "DATE";
		break;
	case SYBTIME:
		fmt = "TIME";
		break;
	case SYBBIT:
		fmt = "BIT";
		break;
	case SYBTEXT:
		fmt = "TEXT";
		break;
	case SYBLONGBINARY:	/* TODO correct ?? */
	case SYBIMAGE:
		fmt = "IMAGE";
		break;
	case SYBMONEY4:
		fmt = "SMALLMONEY";
		break;
	case SYBMONEY:
		fmt = "MONEY";
		break;
	case SYBDATETIME4:
		fmt = "SMALLDATETIME";
		break;
	case SYBREAL:
		fmt = "REAL";
		break;
	case SYBBINARY:
	case XSYBBINARY:
		fmt = "BINARY(%u)";
		break;
	case SYBVARBINARY:
	case XSYBVARBINARY:
		if (curcol->column_varint_size == 8)
			fmt = "VARBINARY(MAX)";
		else
			fmt = "VARBINARY(%u)";
		break;
	case SYBNUMERIC:
		fmt = "NUMERIC(%d,%d)";
		goto numeric_decimal;
	case SYBDECIMAL:
		fmt = "DECIMAL(%d,%d)";
	      numeric_decimal:
		sprintf(out, fmt, curcol->column_prec, curcol->column_scale);
		return TDS_SUCCESS;
		break;
	case SYBUNIQUE:
		if (IS_TDS7_PLUS(tds->conn))
			fmt = "UNIQUEIDENTIFIER";
		break;
	case SYBNTEXT:
		if (IS_TDS7_PLUS(tds->conn))
			fmt = "NTEXT";
		break;
	case SYBNVARCHAR:
	case XSYBNVARCHAR:
		if (curcol->column_varint_size == 8) {
			fmt = "NVARCHAR(MAX)";
		} else if (IS_TDS7_PLUS(tds->conn)) {
			fmt = "NVARCHAR(%u)";
			max_len = 4000;
			size /= 2u;
		}
		break;
	case XSYBNCHAR:
		if (IS_TDS7_PLUS(tds->conn)) {
			fmt = "NCHAR(%u)";
			max_len = 4000;
			size /= 2u;
		}
		break;
	case SYBVARIANT:
		if (IS_TDS7_PLUS(tds->conn))
			fmt = "SQL_VARIANT";
		break;
		/* TODO support scale !! */
	case SYBMSTIME:
		fmt = "TIME";
		break;
	case SYBMSDATE:
		fmt = "DATE";
		break;
	case SYBMSDATETIME2:
		fmt = "DATETIME2";
		break;
	case SYBMSDATETIMEOFFSET:
		fmt = "DATETIMEOFFSET";
		break;
	case SYB5BIGTIME:
		fmt = "BIGTIME";
		break;
	case SYB5BIGDATETIME:
		fmt = "BIGDATETIME";
		break;
	case SYBUINT2:
		fmt = "UNSIGNED SMALLINT";
		break;
	case SYBUINT4:
		fmt = "UNSIGNED INT";
		break;
	case SYBUINT8:
		fmt = "UNSIGNED BIGINT";
		break;
		/* nullable types should not occur here... */
	case SYBFLTN:
	case SYBMONEYN:
	case SYBDATETIMN:
	case SYBBITN:
	case SYBINTN:
		assert(0);
		/* TODO... */
	case SYBVOID:
	case SYBSINT1:
	default:
		tdsdump_log(TDS_DBG_ERROR, "Unknown type %d\n", tds_get_conversion_type(curcol->on_server.column_type, curcol->on_server.column_size));
		break;
	}

	if (fmt) {
		/* fill out */
		sprintf(out, fmt, size > 0 ? MIN(size, max_len) : 1u);
		return TDS_SUCCESS;
	}

	out[0] = 0;
	return TDS_FAIL;
}

/**
 * Write string with parameters definition, useful for TDS7+.
 * Looks like "@P1 INT, @P2 VARCHAR(100)"
 * \param tds     state information for the socket and the TDS protocol
 * \param converted_query     query to send to server in ucs2le encoding
 * \param converted_query_len query length in bytes
 * \param params  parameters to build declaration
 * \return result of write
 */
/* TODO find a better name for this function */
static TDSRET
tds7_write_param_def_from_query(TDSSOCKET * tds, const char* converted_query, size_t converted_query_len, TDSPARAMINFO * params)
{
	char declaration[128], *p;
	int i, count;
	size_t written;
	TDSFREEZE outer, inner;

	assert(IS_TDS7_PLUS(tds->conn));

	count = tds_count_placeholders_ucs2le(converted_query, converted_query + converted_query_len);

	/* string with parameters types */
	tds_put_byte(tds, 0);
	tds_put_byte(tds, 0);
	tds_put_byte(tds, SYBNTEXT);	/* must be Ntype */

	/* put parameters definitions */
	tds_freeze(tds, &outer, 4);
	if (IS_TDS71_PLUS(tds->conn))
		tds_put_n(tds, tds->conn->collation, 5);
	tds_freeze(tds, &inner, 4);

	for (i = 0; i < count; ++i) {
		p = declaration;
		if (i)
			*p++ = ',';

		/* get this parameter declaration */
		p += sprintf(p, "@P%d ", i+1);
		if (!params || i >= params->num_cols) {
			strcpy(p, "varchar(4000)");
		} else if (TDS_FAILED(tds_get_column_declaration(tds, params->columns[i], p))) {
			tds_freeze_abort(&inner);
			tds_freeze_abort(&outer);
			return TDS_FAIL;
		}

		tds_put_string(tds, declaration, -1);
	}

	written = tds_freeze_written(&inner) - 4;
	tds_freeze_close_len(&inner, written ? written : -1);
	tds_freeze_close_len(&outer, written);
	return TDS_SUCCESS;
}

/**
 * Write string with parameters definition, useful for TDS7+.
 * Looks like "@P1 INT, @P2 VARCHAR(100)"
 * \param tds       state information for the socket and the TDS protocol
 * \param query     query to send to server encoded in ucs2le
 * \param query_len query length in bytes
 * \param params    parameters to build declaration
 * \return result of the operation
 */
/* TODO find a better name for this function */
static TDSRET
tds7_write_param_def_from_params(TDSSOCKET * tds, const char* query, size_t query_len, TDSPARAMINFO * params)
{
	char declaration[40];
	int i;
	struct tds_ids {
		const char *p;
		size_t len;
	} *ids = NULL;
	TDSFREEZE outer, inner;
	size_t written;

	assert(IS_TDS7_PLUS(tds->conn));

	/* string with parameters types */
	tds_put_byte(tds, 0);
	tds_put_byte(tds, 0);
	tds_put_byte(tds, SYBNTEXT);	/* must be Ntype */

	/* put parameters definitions */
	tds_freeze(tds, &outer, 4);
	if (IS_TDS71_PLUS(tds->conn))
		tds_put_n(tds, tds->conn->collation, 5);
	tds_freeze(tds, &inner, 4);

	if (!params || !params->num_cols) {
		tds_freeze_close_len(&inner, -1);
		tds_freeze_close_len(&outer, 0);
		return TDS_SUCCESS;
	}

	/* try to detect missing names */
	ids = tds_new0(struct tds_ids, params->num_cols);
	if (!ids)
		goto Cleanup;
	if (tds_dstr_isempty(&params->columns[0]->column_name)) {
		const char *s = query, *e, *id_end;
		const char *query_end = query + query_len;

		for (i = 0;  i < params->num_cols; s = e + 2) {
			e = tds_next_placeholder_ucs2le(s, query_end, 1);
			if (e == query_end)
				break;
			if (e[0] != '@')
				continue;
			/* find end of param name */
			for (id_end = e + 2; id_end != query_end; id_end += 2)
				if (!id_end[1] && (id_end[0] != '_' && id_end[1] != '#' && !isalnum((unsigned char) id_end[0])))
					break;
			ids[i].p = e;
			ids[i].len = id_end - e;
			++i;
		}
	}

	for (i = 0; i < params->num_cols; ++i) {
		if (i)
			tds_put_smallint(tds, ',');

		/* this part of buffer can be not-ascii compatible, use all ucs2... */
		if (ids[i].p) {
			tds_put_n(tds, ids[i].p, ids[i].len);
		} else {
			tds_put_string(tds, tds_dstr_cstr(&params->columns[i]->column_name),
				       tds_dstr_len(&params->columns[i]->column_name));
		}
		tds_put_smallint(tds, ' ');

		/* get this parameter declaration */
		tds_get_column_declaration(tds, params->columns[i], declaration);
		if (!declaration[0])
			goto Cleanup;
		tds_put_string(tds, declaration, -1);
	}
	free(ids);

	written = tds_freeze_written(&inner) - 4;
	tds_freeze_close_len(&inner, written);
	tds_freeze_close_len(&outer, written);

	return TDS_SUCCESS;

      Cleanup:
	free(ids);
	tds_freeze_abort(&inner);
	tds_freeze_abort(&outer);
	return TDS_FAIL;
}


/**
 * Output params types and query (required by sp_prepare/sp_executesql/sp_prepexec)
 * \param tds       state information for the socket and the TDS protocol
 * \param query     query (encoded in ucs2le)
 * \param query_len query length in bytes
 */
static void
tds7_put_query_params(TDSSOCKET * tds, const char *query, size_t query_len)
{
	size_t len;
	int i, num_placeholders;
	const char *s, *e;
	char buf[24];
	const char *const query_end = query + query_len;

	assert(IS_TDS7_PLUS(tds->conn));

	/* we use all "@PX" for parameters */
	num_placeholders = tds_count_placeholders_ucs2le(query, query_end);
	len = num_placeholders * 2;
	/* adjust for the length of X */
	for (i = 10; i <= num_placeholders; i *= 10) {
		len += num_placeholders - i + 1;
	}

	/* string with sql statement */
	/* replace placeholders with dummy parametes */
	tds_put_byte(tds, 0);
	tds_put_byte(tds, 0);
	tds_put_byte(tds, SYBNTEXT);	/* must be Ntype */
	len = 2u * len + query_len;
	TDS_PUT_INT(tds, len);
	if (IS_TDS71_PLUS(tds->conn))
		tds_put_n(tds, tds->conn->collation, 5);
	TDS_PUT_INT(tds, len);
	s = query;
	/* TODO do a test with "...?" and "...?)" */
	for (i = 1;; ++i) {
		e = tds_next_placeholder_ucs2le(s, query_end, 0);
		assert(e && query <= e && e <= query_end);
		tds_put_n(tds, s, e - s);
		if (e == query_end)
			break;
		sprintf(buf, "@P%d", i);
		tds_put_string(tds, buf, -1);
		s = e + 2;
	}
}

/**
 * Get column size for wire
 */
size_t
tds_fix_column_size(TDSSOCKET * tds, TDSCOLUMN * curcol)
{
	size_t size = curcol->on_server.column_size, min;

	if (!size) {
		size = curcol->column_size;
		if (is_unicode_type(curcol->on_server.column_type))
			size *= 2u;
	}

	switch (curcol->column_varint_size) {
	case 1:
		size = MAX(MIN(size, 255), 1);
		break;
	case 2:
		/* note that varchar(max)/varbinary(max) have a varint of 8 */
		if (curcol->on_server.column_type == XSYBNVARCHAR || curcol->on_server.column_type == XSYBNCHAR)
			min = 2;
		else
			min = 1;
		size = MAX(MIN(size, 8000u), min);
		break;
	case 4:
		if (curcol->on_server.column_type == SYBNTEXT)
			size = 0x7ffffffeu;
		else
			size = 0x7fffffffu;
		break;
	default:
		break;
	}
	return size;
}

/**
 * Put data information to wire
 * \param tds    state information for the socket and the TDS protocol
 * \param curcol column where to store information
 * \param flags  bit flags on how to send data (use TDS_PUT_DATA_USE_NAME for use name information)
 * \return TDS_SUCCESS or TDS_FAIL
 */
static TDSRET
tds_put_data_info(TDSSOCKET * tds, TDSCOLUMN * curcol, int flags)
{
	int len;


	if (flags & TDS_PUT_DATA_USE_NAME) {
		len = tds_dstr_len(&curcol->column_name);
		tdsdump_log(TDS_DBG_ERROR, "tds_put_data_info putting param_name \n");

		if (IS_TDS7_PLUS(tds->conn)) {
			TDSFREEZE outer;
			size_t written;

			tds_freeze(tds, &outer, 1);
			if ((flags & TDS_PUT_DATA_PREFIX_NAME) != 0)
				tds_put_smallint(tds, '@');
			tds_put_string(tds, tds_dstr_cstr(&curcol->column_name), len);
			written = (tds_freeze_written(&outer) - 1) / 2;
			tds_freeze_close_len(&outer, written);
		} else {
			TDS_START_LEN_TINYINT(tds) { /* param name len */
				tds_put_string(tds, tds_dstr_cstr(&curcol->column_name), len);
			} TDS_END_LEN
		}
	} else {
		tds_put_byte(tds, 0x00);	/* param name len */
	}
	/*
	 * TODO support other flags (use defaul null/no metadata)
	 * bit 1 (2 as flag) in TDS7+ is "default value" bit 
	 * (what's the meaning of "default value" ?)
	 */

	tdsdump_log(TDS_DBG_ERROR, "tds_put_data_info putting status \n");
	if (flags & TDS_PUT_DATA_LONG_STATUS)
		tds_put_int(tds, curcol->column_output);	/* status (input) */
	else
		tds_put_byte(tds, curcol->column_output);	/* status (input) */
	if (!IS_TDS7_PLUS(tds->conn))
		tds_put_int(tds, curcol->column_usertype);	/* usertype */
	tds_put_byte(tds, curcol->on_server.column_type);

	if (curcol->funcs->put_info(tds, curcol) != TDS_SUCCESS)
		return TDS_FAIL;

	/* TODO needed in TDS4.2 ?? now is called only if TDS >= 5 */
	if (!IS_TDS7_PLUS(tds->conn))
		tds_put_byte(tds, 0x00);	/* locale info length */

	return TDS_SUCCESS;
}

/**
 * tds_send_cancel() sends an empty packet (8 byte header only)
 * tds_process_cancel should be called directly after this.
 * \param tds state information for the socket and the TDS protocol
 * \remarks
 *	tcp will either deliver the packet or time out. 
 *	(TIME_WAIT determines how long it waits between retries.)  
 *	
 *	On sending the cancel, we may get EAGAIN.  We then select(2) until we know
 *	either 1) it succeeded or 2) it didn't.  On failure, close the socket,
 *	tell the app, and fail the function.  
 *	
 *	On success, we read(2) and wait for a reply with select(2).  If we get
 *	one, great.  If the client's timeout expires, we tell him, but all we can
 *	do is wait some more or give up and close the connection.  If he tells us
 *	to cancel again, we wait some more.  
 */
TDSRET
tds_send_cancel(TDSSOCKET * tds)
{
#if ENABLE_ODBC_MARS
	CHECK_TDS_EXTRA(tds);

	tdsdump_log(TDS_DBG_FUNC, "tds_send_cancel: %sin_cancel and %sidle\n", 
				(tds->in_cancel? "":"not "), (tds->state == TDS_IDLE? "":"not "));

	/* one cancel is sufficient */
	if (tds->in_cancel || tds->state == TDS_IDLE) {
		return TDS_SUCCESS;
	}

	tds->in_cancel = 1;

	if (tds_mutex_trylock(&tds->conn->list_mtx)) {
		/* TODO check */
		/* signal other socket */
		tds_wakeup_send(&tds->conn->wakeup, 1);
		return TDS_SUCCESS;
	}
	if (tds->conn->in_net_tds) {
		tds_mutex_unlock(&tds->conn->list_mtx);
		/* TODO check */
		/* signal other socket */
		tds_wakeup_send(&tds->conn->wakeup, 1);
		return TDS_SUCCESS;
	}
	tds_mutex_unlock(&tds->conn->list_mtx);

	/*
	problem: if we are in in_net and we got a signal ??
	on timeout we and a cancel, directly in in_net
	if we hold the lock and we get a signal lock create a death lock

	if we use a recursive mutex and we can get the lock there are 2 cases
	- same thread, we could add a packet and signal, no try ok
	- first thread locking, we could add a packet but are we sure it get processed ??, no try ok
	if recursive mutex and we can't get another thread, wait

	if mutex is not recursive and we get the lock (try)
	- nobody locked, if in_net it could be same or another
	if mutex is not recursive and we can't get the lock
	- another thread is locking, sending signal require not exiting and global list (not protected by list_mtx)
	- same thread have lock, we can't wait nothing without deathlock, setting a flag in tds and signaling could help

	if a tds is waiting for data or is waiting for a condition or for a signal in poll
	pass cancel request on socket ??
	 */

	tds->out_flag = TDS_CANCEL;
	tds->out_pos = 8;
 	tdsdump_log(TDS_DBG_FUNC, "tds_send_cancel: sending cancel packet\n");
	return tds_flush_packet(tds);
#else
	TDSRET rc;

	/*
	 * if we are not able to get the lock signal other thread
	 * this means that either:
	 * - another thread is processing data
	 * - we got called from a signal inside processing thread
	 * - we got called from message handler
	 */
	if (tds_mutex_trylock(&tds->wire_mtx)) {
		/* TODO check */
		if (!tds->in_cancel)
			tds->in_cancel = 1;
		/* signal other socket */
		tds_wakeup_send(&tds->conn->wakeup, 1);
		return TDS_SUCCESS;
	}

	tdsdump_log(TDS_DBG_FUNC, "tds_send_cancel: %sin_cancel and %sidle\n", 
				(tds->in_cancel? "":"not "), (tds->state == TDS_IDLE? "":"not "));

	/* one cancel is sufficient */
	if (tds->in_cancel || tds->state == TDS_IDLE) {
		tds_mutex_unlock(&tds->wire_mtx);
		return TDS_SUCCESS;
	}

	rc = tds_put_cancel(tds);
	tds_mutex_unlock(&tds->wire_mtx);

	return rc;
#endif
}

/**
 * Quote a string properly. Output string is always NUL-terminated
 * \tds
 * \param buffer   output buffer. If NULL function will just return
 *        required bytes
 * \param quoting  quote character (should be one of '\'', '"', ']')
 * \param id       string to quote
 * \param len      length of string to quote
 * \returns size of output string
 */
static size_t
tds_quote(TDSSOCKET * tds, char *buffer, char quoting, const char *id, size_t len)
{
	size_t size;
	const char *src, *pend;
	char *dst;

	pend = id + len;

	/* quote */
	src = id;
	if (!buffer) {
		size = 2u + len;
		for (; src != pend; ++src)
			if (*src == quoting)
				++size;
		return size;
	}

	dst = buffer;
	*dst++ = (quoting == ']') ? '[' : quoting;
	for (; src != pend; ++src) {
		if (*src == quoting)
			*dst++ = quoting;
		*dst++ = *src;
	}
	*dst++ = quoting;
	*dst = 0;
	return dst - buffer;
}

/**
 * Quote an id
 * \param tds    state information for the socket and the TDS protocol
 * \param buffer buffer to store quoted id. If NULL do not write anything 
 *        (useful to compute quote length)
 * \param id     id to quote
 * \param idlen  id length (< 0 for NUL terminated)
 * \result written chars (not including needed terminator)
 * \see tds_quote_id_rpc
 */
size_t
tds_quote_id(TDSSOCKET * tds, char *buffer, const char *id, int idlen)
{
	size_t i, len;

	len = idlen < 0 ? strlen(id) : (size_t) idlen;

	/* quote always for mssql */
	if (TDS_IS_MSSQL(tds) || tds->conn->product_version >= TDS_SYB_VER(12, 5, 1))
		return tds_quote(tds, buffer, ']', id, len);

	/* need quote ?? */
	for (i = 0; i < len; ++i) {
		char c = id[i];

		if (c >= 'a' && c <= 'z')
			continue;
		if (c >= 'A' && c <= 'Z')
			continue;
		if (i > 0 && c >= '0' && c <= '9')
			continue;
		if (c == '_')
			continue;
		return tds_quote(tds, buffer, '\"', id, len);
	}

	if (buffer) {
		memcpy(buffer, id, len);
		buffer[len] = '\0';
	}
	return len;
}

/**
 * Set current cursor.
 * Current cursor is the one will receive output from server.
 * \tds
 * \param cursor  cursor to set as current
 */
static inline void
tds_set_cur_cursor(TDSSOCKET *tds, TDSCURSOR *cursor)
{
	++cursor->ref_count;
	if (tds->cur_cursor)
		tds_release_cursor(&tds->cur_cursor);
	tds->cur_cursor = cursor;
}

TDSRET
tds_cursor_close(TDSSOCKET * tds, TDSCURSOR * cursor)
{
	if (!cursor)
		return TDS_FAIL;

	tdsdump_log(TDS_DBG_INFO1, "tds_cursor_close() cursor id = %d\n", cursor->cursor_id);

	if (tds_set_state(tds, TDS_WRITING) != TDS_WRITING)
		return TDS_FAIL;

	tds_set_cur_cursor(tds, cursor);

	if (IS_TDS50(tds->conn)) {
		tds->out_flag = TDS_NORMAL;
		tds_put_byte(tds, TDS_CURCLOSE_TOKEN);
		tds_put_smallint(tds, 5);	/* length of the data stream that follows */
		tds_put_int(tds, cursor->cursor_id);	/* cursor id returned by the server is available now */

		if (cursor->status.dealloc == TDS_CURSOR_STATE_REQUESTED) {
			tds_put_byte(tds, 0x01);	/* Close option: TDS_CUR_COPT_DEALLOC */
			cursor->status.dealloc = TDS_CURSOR_STATE_SENT;
		}
		else
			tds_put_byte(tds, 0x00);	/* Close option: TDS_CUR_COPT_UNUSED */

	}
	if (IS_TDS7_PLUS(tds->conn)) {

		/* RPC call to sp_cursorclose */
		tds_start_query(tds, TDS_RPC);

		if (IS_TDS71_PLUS(tds->conn)) {
			tds_put_smallint(tds, -1);
			tds_put_smallint(tds, TDS_SP_CURSORCLOSE);
		} else {
			TDS_PUT_N_AS_UCS2(tds, "sp_cursorclose");
		}

		/* This flag tells the SP to output only a dummy metadata token  */

		tds_put_smallint(tds, 2);

		/* input cursor handle (int) */

		tds_put_byte(tds, 0);	/* no parameter name */
		tds_put_byte(tds, 0);	/* input parameter  */
		tds_put_byte(tds, SYBINTN);
		tds_put_byte(tds, 4);
		tds_put_byte(tds, 4);
		tds_put_int(tds, cursor->cursor_id);
		tds->current_op = TDS_OP_CURSORCLOSE;
	}
	return tds_query_flush_packet(tds);

}


/**
 * Check if a cursor is allocated into the server.
 * If is not allocated it assures is removed from the connection list
 * \tds
 * \return true if allocated false otherwise
 */
static bool
tds_cursor_check_allocated(TDSCONNECTION * conn, TDSCURSOR * cursor)
{
	if (cursor->srv_status == TDS_CUR_ISTAT_UNUSED || (cursor->srv_status & TDS_CUR_ISTAT_DEALLOC) != 0
	    || (IS_TDS7_PLUS(conn) && (cursor->srv_status & TDS_CUR_ISTAT_CLOSED) != 0)) {
		tds_cursor_deallocated(conn, cursor);
		return false;
	}

	return true;
}

/**
 * Send a deallocation request to server
 */
TDSRET
tds_cursor_dealloc(TDSSOCKET * tds, TDSCURSOR * cursor)
{
	TDSRET res = TDS_SUCCESS;

	if (!cursor)
		return TDS_FAIL;

	if (!tds_cursor_check_allocated(tds->conn, cursor))
		return TDS_SUCCESS;

	tdsdump_log(TDS_DBG_INFO1, "tds_cursor_dealloc() cursor id = %d\n", cursor->cursor_id);

	if (IS_TDS50(tds->conn)) {
		if (tds_set_state(tds, TDS_WRITING) != TDS_WRITING)
			return TDS_FAIL;
		tds_set_cur_cursor(tds, cursor);

		tds->out_flag = TDS_NORMAL;
		tds_put_byte(tds, TDS_CURCLOSE_TOKEN);
		tds_put_smallint(tds, 5);	/* length of the data stream that follows */
		tds_put_int(tds, cursor->cursor_id);	/* cursor id returned by the server is available now */
		tds_put_byte(tds, 0x01);	/* Close option: TDS_CUR_COPT_DEALLOC */
		res = tds_query_flush_packet(tds);
	}

	/*
	 * in TDS 5 the cursor deallocate function involves
	 * a server interaction. The cursor will be freed
	 * when we receive acknowledgement of the cursor
	 * deallocate from the server. for TDS 7 we do it
	 * here...
	 */
	if (IS_TDS7_PLUS(tds->conn)) {
		if (cursor->status.dealloc == TDS_CURSOR_STATE_SENT ||
			cursor->status.dealloc == TDS_CURSOR_STATE_REQUESTED) {
			tdsdump_log(TDS_DBG_ERROR, "tds_cursor_dealloc(): freeing cursor \n");
		}
	}

	return res;
}

static const TDSCONTEXT empty_ctx = {0};

TDSRET
tds_disconnect(TDSSOCKET * tds)
{
	TDS_INT old_timeout;
	const TDSCONTEXT *old_ctx;
 
	tdsdump_log(TDS_DBG_FUNC, "tds_disconnect() \n");
 
	if (!IS_TDS50(tds->conn))
		return TDS_SUCCESS;

	old_timeout = tds->query_timeout;
	old_ctx = tds_get_ctx(tds);

	/* avoid to stall forever */
	tds->query_timeout = 5;

	/* do not report errors to upper libraries */
	tds_set_ctx(tds, &empty_ctx);

	if (tds_set_state(tds, TDS_WRITING) != TDS_WRITING) {
		tds->query_timeout = old_timeout;
		tds_set_ctx(tds, old_ctx);
		return TDS_FAIL;
	}
 
	tds->out_flag = TDS_NORMAL;
	tds_put_byte(tds, TDS_LOGOUT_TOKEN);
	tds_put_byte(tds, 0);
 
	tds_query_flush_packet(tds);
 
	return tds_process_simple_query(tds);
}
 
/*
 * TODO add function to return type suitable for param
 * ie:
 * sybvarchar -> sybvarchar / xsybvarchar
 * sybint4 -> sybintn
 */

/** @} */
