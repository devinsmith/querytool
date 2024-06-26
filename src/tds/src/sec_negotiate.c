/* FreeTDS - Library of routines accessing Sybase and Microsoft databases
 * Copyright (C) 2015  Frediano Ziglio
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
#include <stdlib.h>

#include <stddef.h>

#include <freetds/tds.h>
#include <freetds/utils/string.h>

#ifdef HAVE_GNUTLS
#  include "sec_negotiate_gnutls.h"
#endif


/**
 * \ingroup libtds
 * \defgroup auth Authentication
 * Functions for handling authentication.
 */

/**
 * \addtogroup auth
 * @{
 */

#if defined(HAVE_GNUTLS) || defined(HAVE_OPENSSL)

typedef struct tds5_negotiate
{
	TDSAUTHENTICATION tds_auth;
} TDS5NEGOTIATE;

/**
 * Initialize Sybase negotiate handling
 * @param tds     A pointer to the TDSSOCKET structure managing a client/server operation.
 * @return authentication info
 */
TDSAUTHENTICATION *
tds5_negotiate_get_auth(TDSSOCKET * tds)
{
	tdsdump_log(TDS_DBG_ERROR,
		"Sybase authentication not supported if GnuTLS or OpenSSL are not present\n");

	return NULL;
}

#endif

/** @} */

