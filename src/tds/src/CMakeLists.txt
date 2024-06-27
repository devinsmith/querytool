
add_library(tds STATIC
  charset_lookup.h
  mem.c token.c util.c login.cpp read.c
  write.c convert.c numeric.c config.cpp query.c iconv.c
  locale.c
  getmac.cpp data.c net.c tls.c
  tds_checks.c log.c encodings.h
  packet.cpp stream.c random.c tds_types.h
  sec_negotiate_gnutls.h sec_negotiate_openssl.h sec_negotiate.c gssapi.c challenge.c
  md4.c md5.c des.c hmac_md5.c threadsafe.c
  tdsstring.c
  strlcpy.c
)

if (NOT WIN32)
	set_target_properties(tds PROPERTIES POSITION_INDEPENDENT_CODE ON)
endif()

