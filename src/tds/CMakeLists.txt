# The minimum version is the version supported by the oldest Debian release
# Ubuntu 20.04LTS uses 3.16.3
cmake_minimum_required(VERSION 3.13.4)
project(tds)

option(WITH_OPENSSL        "Link in OpenSSL if found" ON)
option(ENABLE_KRB5         "Enable Kerberos support" OFF)

if(COMMAND cmake_policy)
	cmake_policy(SET CMP0003 NEW)
	cmake_policy(SET CMP0005 NEW)
endif(COMMAND cmake_policy)

set(FREETDS_TOPDIR ${CMAKE_CURRENT_LIST_DIR})

# TODO depends on configure
add_definitions(-D_REENTRANT -D_THREAD_SAFE)

include(CheckIncludeFile)
include(CheckIncludeFiles)
include(CheckTypeSize)
include(CheckFunctionExists)
include(CheckLibraryExists)
include(CheckStructHasMember)
include(CheckPrototypeDefinition)

find_package(PkgConfig)

macro(config_write str)
	file(APPEND "${CMAKE_BINARY_DIR}/include/config.h.in" "${str}")
endmacro()

file(WRITE "${CMAKE_BINARY_DIR}/include/config.h.in" "/* Automatic generated by cmake */\n\n#define _freetds_config_h_\n")

foreach(fn
	arpa/inet.h
	errno.h
	inttypes.h
	langinfo.h
	limits.h
	locale.h
	netdb.h
	netinet/in.h
	netinet/tcp.h
	paths.h
	poll.h
	stdlib.h
	string.h
	sys/eventfd.h
	sys/ioctl.h
	sys/stat.h
	sys/time.h
	sys/types.h
	unistd.h
	fcntl.h
	wchar.h)

	string(REGEX REPLACE "[/.]" "_" var "${fn}")
	string(TOUPPER "HAVE_${var}" var)
	CHECK_INCLUDE_FILE(${fn} ${var})
	config_write("/* Define to 1 if you have the <${fn}> header file. */\n")
	config_write("#cmakedefine ${var} 1\n\n")
endforeach(fn)

macro(CHECK_FUNCTION_EXISTS_DEFINE func)
	string(TOUPPER "HAVE_${func}" var)
	check_function_exists(${func} ${var})
	config_write("/* Define to 1 if you have function <${func}>. */\n")
	config_write("#cmakedefine ${var} 1\n\n")
endmacro()

if(WITH_OPENSSL)
	find_package(OpenSSL)
endif(WITH_OPENSSL)

if(OPENSSL_FOUND)
	config_write("#define HAVE_OPENSSL 1\n\n")
	include_directories(${OPENSSL_INCLUDE_DIR})
	set(CMAKE_REQUIRED_LIBRARIES ${OPENSSL_LIBRARIES})
	# This fixes an issue with OpenSSL compiled as static library
	if(WIN32)
		list(APPEND CMAKE_REQUIRED_LIBRARIES crypt32 ws2_32)
	endif(WIN32)
	check_function_exists_define(BIO_get_data)
	check_function_exists_define(RSA_get0_key)
	check_function_exists_define(ASN1_STRING_get0_data)
	set(CMAKE_REQUIRED_LIBRARIES)
endif(OPENSSL_FOUND)

set(CMAKE_THREAD_PREFER_PTHREAD ON)
find_package(Threads REQUIRED)

set(CMAKE_EXTRA_INCLUDE_FILES)

# find functions
set(CMAKE_REQUIRED_LIBRARIES ${CMAKE_THREAD_LIBS_INIT})
set(FUNCS strlcpy gettimeofday
	nl_langinfo getuid getpwuid getpwuid_r alarm
	getaddrinfo inet_ntop gethostname poll socketpair
	clock_gettime pthread_cond_timedwait pthread_cond_timedwait_relative_np
	pthread_condattr_setclock readdir_r eventfd)

# TODO
set(HAVE_GETADDRINFO 1 CACHE INTERNAL "")

foreach(func ${FUNCS})
	check_function_exists_define(${func})
endforeach(func)

set(CMAKE_REQUIRED_LIBRARIES)

check_struct_has_member("struct tm" "tm_zone" "time.h" HAVE_STRUCT_TM_TM_ZONE)
config_write("#cmakedefine HAVE_STRUCT_TM_TM_ZONE 1\n\n")

macro(SEARCH_LIBRARY FUNC HAVE VAR LIBS)
	foreach(lib ${LIBS})
		if(NOT ${HAVE})
			unset(${HAVE} CACHE)
			check_library_exists(${lib} ${FUNC} "" ${HAVE})
			if(${HAVE})
				unset(${VAR} CACHE)
				set(${VAR} ${lib} CACHE INTERNAL "")
			endif()
		endif()
	endforeach(lib)
endmacro(SEARCH_LIBRARY)

# flags
foreach(flag EXTRA_CHECKS KRB5)
	config_write("#cmakedefine ENABLE_${flag} 1\n\n")
endforeach(flag)

config_write("#define VERSION \"\\@VERSION\\@\"\n\n")

config_write("#define FREETDS_TOPDIR \"${FREETDS_TOPDIR}\"\n\n")

# TODO allow to configure default TDS protocol version

config_write("#ifdef _MSC_VER
#  define inline __inline
#  define _CRT_SECURE_NO_WARNINGS 1
#endif\n\n")

	# this section make just work on Linux, many definition are not checked
	config_write("#define HAVE_FUNC_GETPWUID_R_5 1\n\n")
	config_write("#define HAVE_FUNC_LOCALTIME_R_TM 1\n\n")
	config_write("/* define to format string used for 64bit integers */\n#define TDS_I64_PREFIX \"ll\"\n\n")
	config_write("#define _GNU_SOURCE 1\n\n")

	if(NOT OPENSSL_FOUND)
		include(FindGnuTLS)
		if(GNUTLS_FOUND)
			config_write("#define HAVE_GNUTLS 1\n")
			set(CMAKE_REQUIRED_LIBRARIES ${GNUTLS_LIBRARIES})
			check_function_exists_define(gnutls_certificate_set_verify_function)
			check_function_exists_define(gnutls_record_disable_padding)
			check_function_exists_define(gnutls_rnd)
			set(CMAKE_REQUIRED_LIBRARIES)
			pkg_check_modules(NETTLEDEP gnutls)
			if (";${NETTLEDEP_STATIC_LIBRARIES};" MATCHES ";nettle;")
				set(lib_GNUTLS ${GNUTLS_LIBRARIES})
				config_write("#define GNUTLS_USE_NETTLE 1\n\n")
				pkg_check_modules(NETTLE nettle)
				if(NETTLE_FOUND)
					set(lib_GNUTLS ${GNUTLS_LIBRARIES} ${NETTLE_LIBRARIES})
					config_write("#define HAVE_NETTLE 1\n\n")
					if (";${NETTLEDEP_STATIC_LIBRARIES};" MATCHES ";hogweed;")
						set(lib_GNUTLS ${lib_GNUTLS} hogweed)
					endif()
					if (";${NETTLEDEP_STATIC_LIBRARIES};" MATCHES ";gmp;")
						set(lib_GNUTLS ${lib_GNUTLS} gmp)
						config_write("#define HAVE_GMP 1\n\n")
					endif()
				endif(NETTLE_FOUND)
			else()
				set(lib_GNUTLS ${GNUTLS_LIBRARIES} gcrypt tasn1)
			endif()
		endif(GNUTLS_FOUND)
	endif(NOT OPENSSL_FOUND)

	if(NOT HAVE_CLOCK_GETTIME AND NOT HAVE_GETHRTIME)
		search_library(clock_gettime HAVE_CLOCK_GETTIME lib_RT "rt;posix4")
		config_write("#cmakedefine HAVE_CLOCK_GETTIME 1\n\n")
	endif(NOT HAVE_CLOCK_GETTIME AND NOT HAVE_GETHRTIME)

	if(CMAKE_USE_PTHREADS_INIT)
		config_write("#define HAVE_PTHREAD 1\n\n")
		config_write("#define TDS_HAVE_PTHREAD_MUTEX 1\n\n")
	endif(CMAKE_USE_PTHREADS_INIT)

config_write("

#define TIME_WITH_SYS_TIME 1
//#define _ALL_SOURCE 1
#define __EXTENSIONS__ 1

// iconv
#define HAVE_ICONV 1
#define ICONV_CONST
")

set(lib_BASE ${lib_RT} ${CMAKE_THREAD_LIBS_INIT})

if(WIN32)
	set(lib_NETWORK ws2_32 crypt32)
else(WIN32)
  # TODO check libraries
  if (ENABLE_KRB5)
    set(lib_NETWORK gssapi_krb5)
  endif()
endif(WIN32)

if(OPENSSL_FOUND)
	set(lib_NETWORK ${lib_NETWORK} ${OPENSSL_LIBRARIES})
elseif(GNUTLS_FOUND)
	set(lib_NETWORK ${lib_NETWORK} ${lib_GNUTLS})
endif(OPENSSL_FOUND)

include_directories(${CMAKE_BINARY_DIR}/include include win32 iconv/include)

configure_file(${CMAKE_BINARY_DIR}/include/config.h.in ${CMAKE_BINARY_DIR}/include/config.h)

add_subdirectory(src)


