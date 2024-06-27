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

#include <stdio.h>

#include <assert.h>

#include <stdlib.h>

#include <string.h>

#include <unistd.h>

#include <netdb.h>

#include <sys/socket.h>

#include <sys/types.h>


#include <freetds/tds.h>
#include <freetds/utils/string.h>

static int tds_config_login(TDSLOGIN * connection, TDSLOGIN * login);
static int tds_config_env_tdsdump(TDSLOGIN * login);
static int tds_config_env_tdshost(TDSLOGIN * login);
static bool tds_read_interfaces(const char *server, TDSLOGIN * login);
static bool parse_server_name_for_port(TDSLOGIN * connection, TDSLOGIN * login, bool update_server);

const char STD_DATETIME_FMT[] = "%b %e %Y %I:%M%p";

static const char pid_config_logpath[] = "/tmp/tdsconfig.log.%d";
static const char pid_logpath[] = "/tmp/freetds.log.%d";

/**
 * \ingroup libtds
 * \defgroup config Configuration
 * Handle reading of configuration
 */

/**
 * \addtogroup config
 * @{ 
 */

/**
 * tds_read_config_info() will fill the tds connection structure based on configuration 
 * information gathered in the following order:
 * 1) Program specified in TDSLOGIN structure
 * 2) The environment variables TDSVER, TDSDUMP, TDSPORT, TDSQUERY, TDSHOST
 * 3) A config file with the following search order:
 *    a) a readable file specified by environment variable FREETDSCONF
 *    b) a readable file in ~/.freetds.conf
 *    c) a readable file in $prefix/etc/freetds.conf
 * 3) ~/.interfaces if exists
 * 4) $SYBASE/interfaces if exists
 * 5) TDS_DEF_* default values
 *
 * .tdsrc and freetds.conf have been added to make the package easier to 
 * integration with various Linux and *BSD distributions.
 */
TDSLOGIN *
tds_read_config_info(TDSSOCKET * tds, TDSLOGIN * login, TDSLOCALE * locale)
{
	int opened = 0;

	/* allocate a new structure with hard coded and build-time defaults */
	TDSLOGIN *connection = tds_alloc_login();
	if (!connection || !tds_init_login(connection, locale)) {
		tds_free_login(connection);
		return nullptr;
	}

	const char *s = getenv("TDSDUMPCONFIG");
	if (s) {
		if (*s) {
			opened = tdsdump_open(s);
		} else {
			pid_t pid = getpid();
      char *path;
			if (asprintf(&path, pid_config_logpath, (int) pid) >= 0) {
				if (*path) {
					opened = tdsdump_open(path);
				}
				free(path);
			}
		}
	}

	tdsdump_log(TDS_DBG_INFO1, "Getting connection information for [%s].\n", 
			    tds_dstr_cstr(&login->server_name));	/* (The server name is set in login.c.) */

	/* Read the config files. */
	tdsdump_log(TDS_DBG_INFO1, "Attempting to read conf files.\n");
	bool found = false;
  if (parse_server_name_for_port(connection, login, true)) {

    found = false;
    /* do it again to really override what found in freetds.conf */
    parse_server_name_for_port(connection, login, false);
    if (TDS_SUCCEED(tds_lookup_host_set(tds_dstr_cstr(&connection->server_name), &connection->ip_addrs))) {
      if (!tds_dstr_dup(&connection->server_host_name, &connection->server_name)) {
        tds_free_login(connection);
        return nullptr;
      }
      found = true;
    }
    if (!tds_dstr_dup(&login->server_name, &connection->server_name)) {
      tds_free_login(connection);
      return nullptr;
    }
  }

	if (!found) {
		/* fallback to interfaces file */
		tdsdump_log(TDS_DBG_INFO1, "Failed in reading conf file.  Trying interface files.\n");
		if (!tds_read_interfaces(tds_dstr_cstr(&login->server_name), connection)) {
			tdsdump_log(TDS_DBG_INFO1, "Failed to find [%s] in configuration files; trying '%s' instead.\n", 
						   tds_dstr_cstr(&login->server_name), tds_dstr_cstr(&connection->server_name));
			if (connection->ip_addrs == nullptr)
				tdserror(tds_get_ctx(tds), tds, TDSEINTF, 0);
		}
	}

	/* Override config file settings with environment variables. */
	tds_fix_login(connection);

	/* And finally apply anything from the login structure */
	if (!tds_config_login(connection, login)) {
		tds_free_login(connection);
		return nullptr;
	}
	
	if (opened) {
		char tmp[128];

		tdsdump_log(TDS_DBG_INFO1, "Final connection parameters:\n");
		tdsdump_log(TDS_DBG_INFO1, "\t%20s = %s\n", "server_name", tds_dstr_cstr(&connection->server_name));
		tdsdump_log(TDS_DBG_INFO1, "\t%20s = %s\n", "server_host_name", tds_dstr_cstr(&connection->server_host_name));

		for (struct addrinfo *addrs = connection->ip_addrs; addrs != nullptr; addrs = addrs->ai_next)
			tdsdump_log(TDS_DBG_INFO1, "\t%20s = %s\n", "ip_addr", tds_addrinfo2str(addrs, tmp, sizeof(tmp)));

		if (connection->ip_addrs == nullptr)
			tdsdump_log(TDS_DBG_INFO1, "\t%20s = %s\n", "ip_addr", "");

		tdsdump_log(TDS_DBG_INFO1, "\t%20s = %s\n", "instance_name", tds_dstr_cstr(&connection->instance_name));
		tdsdump_log(TDS_DBG_INFO1, "\t%20s = %d\n", "port", connection->port);
		tdsdump_log(TDS_DBG_INFO1, "\t%20s = %d\n", "major_version", TDS_MAJOR(connection));
		tdsdump_log(TDS_DBG_INFO1, "\t%20s = %d\n", "minor_version", TDS_MINOR(connection));
		tdsdump_log(TDS_DBG_INFO1, "\t%20s = %d\n", "block_size", connection->block_size);
		tdsdump_log(TDS_DBG_INFO1, "\t%20s = %s\n", "language", tds_dstr_cstr(&connection->language));
		tdsdump_log(TDS_DBG_INFO1, "\t%20s = %s\n", "server_charset", tds_dstr_cstr(&connection->server_charset));
		tdsdump_log(TDS_DBG_INFO1, "\t%20s = %d\n", "connect_timeout", connection->connect_timeout);
		tdsdump_log(TDS_DBG_INFO1, "\t%20s = %s\n", "client_host_name", tds_dstr_cstr(&connection->client_host_name));
		tdsdump_log(TDS_DBG_INFO1, "\t%20s = %s\n", "client_charset", tds_dstr_cstr(&connection->client_charset));
		tdsdump_log(TDS_DBG_INFO1, "\t%20s = %d\n", "use_utf16", connection->use_utf16);
		tdsdump_log(TDS_DBG_INFO1, "\t%20s = %s\n", "app_name", tds_dstr_cstr(&connection->app_name));
		tdsdump_log(TDS_DBG_INFO1, "\t%20s = %s\n", "user_name", tds_dstr_cstr(&connection->user_name));
		/* tdsdump_log(TDS_DBG_PASSWD, "\t%20s = %s\n", "password", tds_dstr_cstr(&connection->password)); 
			(no such flag yet) */
		tdsdump_log(TDS_DBG_INFO1, "\t%20s = %s\n", "library", tds_dstr_cstr(&connection->library));
		tdsdump_log(TDS_DBG_INFO1, "\t%20s = %d\n", "bulk_copy", (int)connection->bulk_copy);
		tdsdump_log(TDS_DBG_INFO1, "\t%20s = %d\n", "suppress_language", (int)connection->suppress_language);
		tdsdump_log(TDS_DBG_INFO1, "\t%20s = %d\n", "encrypt level", (int)connection->encryption_level);
		tdsdump_log(TDS_DBG_INFO1, "\t%20s = %d\n", "query_timeout", connection->query_timeout);
		/* tdsdump_log(TDS_DBG_INFO1, "\t%20s = %s\n", "capabilities", tds_dstr_cstr(&connection->capabilities)); 
			(not null terminated) */
		tdsdump_log(TDS_DBG_INFO1, "\t%20s = %s\n", "database", tds_dstr_cstr(&connection->database));
		tdsdump_log(TDS_DBG_INFO1, "\t%20s = %s\n", "dump_file", tds_dstr_cstr(&connection->dump_file));
		tdsdump_log(TDS_DBG_INFO1, "\t%20s = %x\n", "debug_flags", connection->debug_flags);
		tdsdump_log(TDS_DBG_INFO1, "\t%20s = %d\n", "text_size", connection->text_size);
		tdsdump_log(TDS_DBG_INFO1, "\t%20s = %s\n", "server_realm_name", tds_dstr_cstr(&connection->server_realm_name));
		tdsdump_log(TDS_DBG_INFO1, "\t%20s = %s\n", "server_spn", tds_dstr_cstr(&connection->server_spn));
		tdsdump_log(TDS_DBG_INFO1, "\t%20s = %s\n", "cafile", tds_dstr_cstr(&connection->cafile));
		tdsdump_log(TDS_DBG_INFO1, "\t%20s = %s\n", "crlfile", tds_dstr_cstr(&connection->crlfile));
		tdsdump_log(TDS_DBG_INFO1, "\t%20s = %d\n", "check_ssl_hostname", connection->check_ssl_hostname);
		tdsdump_log(TDS_DBG_INFO1, "\t%20s = %s\n", "db_filename", tds_dstr_cstr(&connection->db_filename));
		tdsdump_log(TDS_DBG_INFO1, "\t%20s = %d\n", "readonly_intent", connection->readonly_intent);
		tdsdump_log(TDS_DBG_INFO1, "\t%20s = %s\n", "openssl_ciphers", tds_dstr_cstr(&connection->openssl_ciphers));

		tdsdump_close();
	}

	/*
	 * If a dump file has been specified, start logging
	 */
	if (!tds_dstr_isempty(&connection->dump_file) && !tdsdump_isopen()) {
		if (connection->debug_flags)
			tds_debug_flags = connection->debug_flags;
		tdsdump_open(tds_dstr_cstr(&connection->dump_file));
	}

	return connection;
}

/**
 * Fix configuration after reading it. 
 * Currently this read some environment variables and replace some options.
 */
void
tds_fix_login(TDSLOGIN * login)
{
	/* Now check the environment variables */
	tds_config_env_tdsdump(login);
	tds_config_env_tdshost(login);
}

static int
tds_config_login(TDSLOGIN * connection, TDSLOGIN * login)
{
	DSTR *res = &login->server_name;

	if (!tds_dstr_isempty(&login->server_name)) {
		if (1 || tds_dstr_isempty(&connection->server_name))
			res = tds_dstr_dup(&connection->server_name, &login->server_name);
	}

	if (login->tds_version)
		connection->tds_version = login->tds_version;

	if (res && !tds_dstr_isempty(&login->language))
		res = tds_dstr_dup(&connection->language, &login->language);

	if (res && !tds_dstr_isempty(&login->server_charset))
		res = tds_dstr_dup(&connection->server_charset, &login->server_charset);

	if (res && !tds_dstr_isempty(&login->client_charset)) {
		res = tds_dstr_dup(&connection->client_charset, &login->client_charset);
		tdsdump_log(TDS_DBG_INFO1, "tds_config_login: %s is %s.\n", "client_charset",
			    tds_dstr_cstr(&connection->client_charset));
	}

	if (!login->use_utf16)
		connection->use_utf16 = login->use_utf16;

	if (res && !tds_dstr_isempty(&login->database)) {
		res = tds_dstr_dup(&connection->database, &login->database);
		tdsdump_log(TDS_DBG_INFO1, "tds_config_login: %s is %s.\n", "database_name",
			    tds_dstr_cstr(&connection->database));
	}

	if (res && !tds_dstr_isempty(&login->client_host_name))
		res = tds_dstr_dup(&connection->client_host_name, &login->client_host_name);

	if (res && !tds_dstr_isempty(&login->app_name))
		res = tds_dstr_dup(&connection->app_name, &login->app_name);

	if (res && !tds_dstr_isempty(&login->user_name))
		res = tds_dstr_dup(&connection->user_name, &login->user_name);

	if (res && !tds_dstr_isempty(&login->password)) {
		/* for security reason clear memory */
		tds_dstr_zero(&connection->password);
		res = tds_dstr_dup(&connection->password, &login->password);
	}

	if (res && !tds_dstr_isempty(&login->library))
		res = tds_dstr_dup(&connection->library, &login->library);

	if (login->encryption_level)
		connection->encryption_level = login->encryption_level;

	if (login->suppress_language)
		connection->suppress_language = 1;

	if (!login->bulk_copy)
		connection->bulk_copy = 0;

	if (login->block_size)
		connection->block_size = login->block_size;

	if (login->gssapi_use_delegation)
		connection->gssapi_use_delegation = login->gssapi_use_delegation;

	if (login->mutual_authentication)
		connection->mutual_authentication = login->mutual_authentication;

	if (login->port)
		connection->port = login->port;

	if (login->connect_timeout)
		connection->connect_timeout = login->connect_timeout;

	if (login->query_timeout)
		connection->query_timeout = login->query_timeout;

	if (!login->check_ssl_hostname)
		connection->check_ssl_hostname = login->check_ssl_hostname;

	if (res && !tds_dstr_isempty(&login->db_filename))
		res = tds_dstr_dup(&connection->db_filename, &login->db_filename);

	if (res && !tds_dstr_isempty(&login->openssl_ciphers))
		res = tds_dstr_dup(&connection->openssl_ciphers, &login->openssl_ciphers);

	if (res && !tds_dstr_isempty(&login->server_spn))
		res = tds_dstr_dup(&connection->server_spn, &login->server_spn);

	/* copy other info not present in configuration file */
	connection->capabilities = login->capabilities;

	if (login->readonly_intent)
		connection->readonly_intent = login->readonly_intent;

	connection->use_new_password = login->use_new_password;

	if (login->use_ntlmv2_specified) {
		connection->use_ntlmv2_specified = login->use_ntlmv2_specified;
		connection->use_ntlmv2 = login->use_ntlmv2;
	}

	if (res)
		res = tds_dstr_dup(&connection->new_password, &login->new_password);

	return res != NULL;
}

static int
tds_config_env_tdsdump(TDSLOGIN * login)
{
	char *s = getenv("TDSDUMP");
	if (!s)
		return 1;

	if (!strlen(s)) {
		char *path;
		pid_t pid = getpid();
		if (asprintf(&path, pid_logpath, (int) pid) < 0)
			return 0;
		if (!tds_dstr_set(&login->dump_file, path)) {
			free(path);
			return 0;
		}
	} else {
		if (!tds_dstr_copy(&login->dump_file, s))
			return 0;
	}
	tdsdump_log(TDS_DBG_INFO1, "Setting 'dump_file' to '%s' from $TDSDUMP.\n", tds_dstr_cstr(&login->dump_file));
	return 1;
}

/* TDSHOST env var, pkleef@openlinksw.com 01/21/02 */
static int
tds_config_env_tdshost(TDSLOGIN * login)
{
	const char *tdshost;
	char tmp[128];
	struct addrinfo *addrs;

	if (!(tdshost = getenv("TDSHOST")))
		return 1;

	if (TDS_FAILED(tds_lookup_host_set(tdshost, &login->ip_addrs))) {
		tdsdump_log(TDS_DBG_WARN, "Name resolution failed for '%s' from $TDSHOST.\n", tdshost);
		return 0;
	}

	if (!tds_dstr_copy(&login->server_host_name, tdshost))
		return 0;
	for (addrs = login->ip_addrs; addrs != NULL; addrs = addrs->ai_next) {
		tdsdump_log(TDS_DBG_INFO1, "Setting IP Address to %s (%s) from $TDSHOST.\n",
			    tds_addrinfo2str(addrs, tmp, sizeof(tmp)), tdshost);
	}
	return 1;
}

/**
 * Get the IP address for a hostname. Store server's IP address 
 * in the string 'ip' in dotted-decimal notation.  (The "hostname" might itself
 * be a dotted-decimal address.  
 *
 * If we can't determine the IP address then 'ip' will be set to empty
 * string.
 */
/* TODO callers seem to set always connection info... change it */
struct addrinfo *
tds_lookup_host(const char *servername)	/* (I) name of the server                  */
{
	struct addrinfo hints, *addr = NULL;
	assert(servername != NULL);

	memset(&hints, '\0', sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

#ifdef AI_ADDRCONFIG
	hints.ai_flags |= AI_ADDRCONFIG;
#endif

	if (getaddrinfo(servername, NULL, &hints, &addr))
		return NULL;
	return addr;
}

TDSRET
tds_lookup_host_set(const char *servername, struct addrinfo **addr)
{
	struct addrinfo *newaddr;
	assert(servername != NULL && addr != NULL);

	if ((newaddr = tds_lookup_host(servername)) != NULL) {
		if (*addr != NULL)
			freeaddrinfo(*addr);
		*addr = newaddr;
		return TDS_SUCCESS;
	}
	return TDS_FAIL;
}

/**
 * Try to find the IP number and port for a (possibly) logical server name.
 *
 * @note This function uses only the interfaces file and is deprecated.
 */
static bool
tds_read_interfaces(const char *server, TDSLOGIN * login)
{
	bool found = false;

  int ip_port;

  /*
   * Make a guess about the port number
   */

  if (login->port == 0) {
    /*
     * Not set in the [global] section of the
     * configure file, take a guess.
     */
    ip_port = TDS_DEF_PORT;
  } else {
    /*
     * Preserve setting from the [global] section
     * of the configure file.
     */
    ip_port = login->port;
  }
  tdsdump_log(TDS_DBG_INFO1, "Setting 'ip_port' to %d as a guess.\n", ip_port);

  /*
   * look up the host
   */

  if (TDS_SUCCEED(tds_lookup_host_set(server, &login->ip_addrs)))
    if (!tds_dstr_copy(&login->server_host_name, server))
      return false;

  login->port = ip_port;

	return found;
}

/**
 * Check the server name to find port info first
 * Warning: connection-> & login-> are all modified when needed
 * \return true when found, else false
 */
static bool
parse_server_name_for_port(TDSLOGIN * connection, TDSLOGIN * login, bool update_server)
{
	const char *pSep;
	const char *server;

	/* seek the ':' in login server_name */
	server = tds_dstr_cstr(&login->server_name);

	/* IPv6 address can be quoted */
	if (server[0] == '[') {
		pSep = strstr(server, "]:");
		if (pSep)
			++pSep;
	} else {
		pSep = strrchr(server, ':');
	}

	if (pSep && pSep != server) {	/* yes, i found it! */
		/* modify connection-> && login->server_name & ->port */
		login->port = connection->port = atoi(pSep + 1);
		tds_dstr_empty(&connection->instance_name);
	} else {
		/* handle instance name */
		pSep = strrchr(server, '\\');
		if (!pSep || pSep == server)
			return false;

		if (!tds_dstr_copy(&connection->instance_name, pSep + 1))
			return false;
		connection->port = 0;
	}

	if (!update_server)
		return true;

	if (server[0] == '[' && pSep > server && pSep[-1] == ']') {
		server++;
		pSep--;
	}
	if (!tds_dstr_copyn(&connection->server_name, server, pSep - server))
		return false;

	return true;
}

/** @} */
