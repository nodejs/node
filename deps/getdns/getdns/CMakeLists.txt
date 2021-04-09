cmake_minimum_required(VERSION 3.5 FATAL_ERROR)

if (POLICY CMP0075)
  cmake_policy(SET CMP0075 NEW)
endif ()

# The following must be set BEFORE doing project() or enable_language().
if (NOT CMAKE_BUILD_TYPE)
  message(STATUS "No build type defined; defaulting to 'Debug'")
  set(CMAKE_BUILD_TYPE "Debug" CACHE STRING
    "The type of build. Possible values are: Debug, Release, RelWithDebInfo and MinSizeRel.")
endif ()

set(PACKAGE "getdns")
set(PACKAGE_NAME "getdns")
set(PACKAGE_VERSION "1.6.0")
set(PACKAGE_BUGREPORT "team@getdnsapi.net")
set(PACKAGE_URL "https://getdnsapi.net")

# Dont forget to put a dash in front of the release candidate!!!
# That is how it is done with semantic versioning!
set(RELEASE_CANDIDATE "")

set(PACKAGE_STRING "${PACKAGE_NAME} ${PACKAGE_VERSION}${RELEASE_CANDIDATE}")
set(PACKAGE_TARNAME "${PACKAGE}-${PACKAGE_VERSION}${RELEASE_CANDIDATE}")

set(GETDNS_VERSION "${PACKAGE_VERSION}${RELEASE_CANDIDATE}")
set(GETDNS_NUMERIC_VERSION 0x01060000)
set(API_VERSION "December 2015")
set(API_NUMERIC_VERSION 0x07df0c00)


# Library version
# ---------------
# current:revision:age
# (binary-api-number):(which-binary-api-version):(how-many-nrs-backwardscompat)
# if source code changes increment revision
# if any interfaces have been added/removed/changed since last update then
#   increment current and set revision to 0
# if any interfaces have been added since the last public release then increment age
# if any interfaces have been removed or changed since the last public release then
#   set age to 0
#
# getdns-0.1.4 had libversion 0:0:0
# getdns-0.1.5 had libversion 1:0:0
# getdns-0.1.6 had libversion 1:1:0
# getdns-0.1.7 had libversion 1:2:1 (but should have had 2:0:1)
# getdns-0.1.8 had libversion 1:3:0 (but should have had 2:1:1)
# getdns-0.2.0 had libversion 2:2:1
# getdns-0.3.0 had libversion 3:3:2
# getdns-0.3.1 had libversion 3:4:2
# getdns-0.3.2 had libversion 3:5:2
# getdns-0.3.3 had libversion 3:6:2
# getdns-0.5.0 had libversion 4:0:3
# getdns-0.5.1 had libversion 4:1:3 (but should have been getdns-0.6.0)
# getdns-0.9.0 had libversion 5:0:4
# getdns-1.0.0 had libversion 5:1:4
# getdns-1.1.0 had libversion 6:0:0
# getdns-1.1.1 had libversion 6:1:0
# getdns-1.1.2 had libversion 7:0:1
# getdns-1.1.3 had libversion 7:1:1
# getdns-1.2.0 had libversion 8:0:2
# getdns-1.2.1 had libversion 8:1:2
# getdns-1.3.0 had libversion 9:0:3
# getdns-1.4.0 had libversion 10:0:0
# getdns-1.4.1 had libversion 10:1:0
# getdns-1.4.2 had libversion 10:2:0
# getdns-1.5.0 had libversion 11:0:1
# getdns-1.5.1 had libversion 11:1:1
# getdns-1.5.2 had libversion 11:2:1
# getdns-1.6.0 has libversion 11:3:1
set(GETDNS_VERSION_CURRENT 11)
set(GETDNS_VERSION_REVISION 3)
set(GETDNS_VERSION_AGE 1)

project(getdns VERSION ${PACKAGE_VERSION} LANGUAGES C)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules/")

include(CheckCSourceRuns)
include(CheckFunctionExists)
include(CheckIncludeFile)
include(CheckLibraryExists)
include(CheckSymbolExists)
include(CheckTypeSize)
include(CMakeDependentOption)
include(CTest)
include(GNUInstallDirs)
include(TestBigEndian)

include(TargetSharedLibraryExports)
include(TargetSharedLibraryVersion)

# Target Platform
if (WIN32 OR MINGW OR MSYS OR CYGWIN)
  set(HOSTOS "windows")
  set(GETDNS_ON_WINDOWS 1)
  set(USE_WINSOCK 1)
elseif (APPLE)
  set(HOSTOS "macos")
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -D_DARWIN_C_SOURCE")
elseif (UNIX)
  set(HOSTOS "unix")

  if (NOT ${CMAKE_SYSTEM_NAME} STREQUAL "FreeBSD")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -D_POSIX_C_SOURCE=200112L -D_XOPEN_SOURCE=600")
  endif ()
  if (${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
    set(LINUX 1)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -D_BSD_SOURCE -D_DEFAULT_SOURCE")
  elseif (${CMAKE_SYSTEM_NAME} STREQUAL "Solaris")
    set(SOLARIS 1)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -D__EXTENSIONS_")
  endif ()
endif ()

test_big_endian(TARGET_IS_BIG_ENDIAN)
set(HAVE_TARGET_ENDIANNESS 1)

# Options.
option(ENABLE_SHARED "Build shared libraries." ON)
option(ENABLE_STATIC "Build static libraries." ON)
if ((NOT ENABLE_SHARED) AND (NOT ENABLE_STATIC))
  message(FATAL_ERROR "You must build either static or shared libraries.")
endif ()

option(ENABLE_DEBUG_ALL "Enable all debugging messages.")
cmake_dependent_option(ENABLE_DEBUG_REQ "Enable request debugging messages." OFF "NOT ENABLE_DEBUG_ALL" ON)
cmake_dependent_option(ENABLE_DEBUG_SCHED "Enable scheduling debugging messages." OFF "NOT ENABLE_DEBUG_ALL" ON)
cmake_dependent_option(ENABLE_DEBUG_STUB "Enable stub debugging messages." OFF "NOT ENABLE_DEBUG_ALL" ON)
cmake_dependent_option(ENABLE_DEBUG_DAEMON "Enable daemon debugging messages." OFF "NOT ENABLE_DEBUG_ALL" ON)
cmake_dependent_option(ENABLE_DEBUG_DNSSEC "Enable DNSSEC debugging messages." OFF "NOT ENABLE_DEBUG_ALL" ON)
cmake_dependent_option(ENABLE_DEBUG_SERVER "Enable server debugging messages." OFF "NOT ENABLE_DEBUG_ALL" ON)
cmake_dependent_option(ENABLE_DEBUG_ANCHOR "Enable anchor debugging messages." OFF "NOT ENABLE_DEBUG_ALL" ON)

option(ENABLE_SHA1 "Enable SHA1 RRSIG support." ON)
option(ENABLE_SHA2 "Enable SHA256 and SHA512 RRSIG support." ON)
option(ENABLE_GOST "Enable GOST support." ON)
option(ENABLE_ECDSA "Enable ECDSA support." ON)
option(ENABLE_DSA "Enable DSA support." ON)
option(ENABLE_ED25519 "Enable ED25519 support." ON)
option(ENABLE_ED448 "Enable ED448 support." ON)

option(ENABLE_DRAFT_MDNS_SUPPORT "Enable draft mdns client support.")
option(ENABLE_EDNS_COOKIES "Enable EDNS cookies." ON)
option(ENABLE_NATIVE_STUB_DNSSEC "Enable native stub DNSSEC support." ON)
option(ENABLE_POLL_EVENTLOOP "Enable default eventloop based on poll." ON)
if (WIN32)
  option(ENABLE_STUB_ONLY "Restrict resolution modes to STUB." ON)
else()
  option(ENABLE_STUB_ONLY "Restrict resolution modes to STUB." OFF)
endif()
option(ENABLE_TCP_FAST_OPEN "Enable use of TCP Fast Open." ON)
option(ENABLE_UNBOUND_EVENT_API "Enable usage of libunbound's event API." ON)

option(BUILD_DOXYGEN "Build source documentation." OFF)
option(BUILD_EXAMPLES "Compile the example programs." OFF)
option(BUILD_GETDNS_QUERY "Compile and install the getdns_query tool." ON)
option(BUILD_GETDNS_SERVER_MON "Compile and install the getdns_server_mon tool." ON)
option(BUILD_STUBBY "Compile and install stubby, the (stub) resolver daemon." OFF)

option(BUILD_LIBEV "Build libev support library if available." ON)
option(BUILD_LIBEVENT2 "Build libevent2 support library if available." ON)
option(BUILD_LIBUV "Build libuv support library available." ON)

option(USE_LIBIDN2 "Use libidn2 if available." ON)
option(USE_GNUTLS "Use GnuTLS for TLS connections." OFF)

# Above names chosen for user consistency. Now define substituted names.
set(REQ_DEBUG ${ENABLE_DEBUG_REQ})
set(SCHED_DEBUG ${ENABLE_DEBUG_SCHED})
set(STUB_DEBUG ${ENABLE_DEBUG_STUB})
set(DAEMON_DEBUG ${ENABLE_DEBUG_DAEMON})
set(SEC_DEBUG ${ENABLE_DEBUG_DNSSEC})
set(SERVER_DEBUG ${ENABLE_DEBUG_SERVER})
set(ANCHOR_DEBUG ${ENABLE_DEBUG_ANCHOR})
set(EDNS_COOKIES ${ENABLE_EDNS_COOKIES})

set(USE_SHA1 ${ENABLE_SHA1})
set(USE_SHA2 ${ENABLE_SHA2})
set(USE_GOST ${ENABLE_GOST})
set(USE_ECDSA ${ENABLE_ECDSA})
set(USE_DSA ${ENABLE_DSA})
set(USE_ED25519 ${ENABLE_ED25519})
set(USE_ED448 ${ENABLE_ED448})

set(HAVE_MDNS_SUPPORT ${ENABLE_DRAFT_MDNS_SUPPORT})
set(STUB_NATIVE_DNSSEC ${ENABLE_NATIVE_STUB_DNSSEC})

set(USE_LIBEV ${BUILD_LIBEV})
set(USE_LIBEVENT2 ${BUILD_LIBEVENT2})
set(USE_LIBUV ${BUILD_LIBUV})

option(ENABLE_DEBUG_KEEP_CONNECTIONS_OPEN "Disable connection idle timeout. Do not enable.")
mark_as_advanced(ENABLE_DEBUG_KEEP_CONNECTIONS_OPEN)
set(KEEP_CONNECTIONS_OPEN_DEBUG ${ENABLE_DEBUG_KEEP_CONNECTIONS_OPEN})

# Options variables.
string(TIMESTAMP timestamp "%Y-%m-%dT%H:%M:%SZ")
set(CURRENT_DATE "${timestamp}" CACHE STRING "Current date of the compilation, set to fixed date for reproducible builds.")
set(DNSSEC_ROADBLOCK_AVOIDANCE ON CACHE BOOL "Enable/disable DNSSEC roadblock avoidance.")
set(FD_SETSIZE "" CACHE STRING "Set maximum file descriptor number that can be used by select.")
set(MAX_UDP_BACKOFF 1000 CACHE STRING "Set the maximum number of messages that can be sent to other upstreams before the upstream which has previously timed out will be tried again.")
if (WIN32)
  # BUG! Don't hardcode the Windows directory and drive.
  set(hostsfile "C:/Windows/System32/Drivers/etc/hosts")
else ()
  set(hostsfile "/etc/hosts")
endif ()
set(PATH_HOSTS "${hostsfile}" CACHE STRING "Set the static table lookup for hostnames path.")
set(PATH_RESOLVCONF "/etc/resolv.conf" CACHE STRING "Set the resolver configuration file path. Not used on Windows, where values are retrieved via GetNetworkParams().")
set(PATH_TRUST_ANCHOR_FILE "${CMAKE_INSTALL_FULL_SYSCONFDIR}/unbound/getdns-root.key" CACHE STRING "Default location of the trust anchor file.")

# Ensure option variables in config.h that get values from the above are
# defined, and so will actually appear in config.h.
set(GETDNS_FN_HOSTS 1)
set(GETDNS_FN_RESOLVCONF 1)
set(TRUST_ANCHOR_FILE 1)
set(UDP_MAX_BACKOFF 1)

# Options not exposed in autoconf.
set(DRAFT_RRTYPES 1)
set(EDNS_COOKIE_OPCODE 10)
set(EDNS_COOKIE_ROLLOVER_TIME "(24*60*60)")
set(EDNS_PADDING_OPCODE 12)
set(MAX_CNAME_REFERRALS 100)
set(MAXIMUM_UPSTREAM_OPTION_SPACE 3000)

# Values derived from options.
set(GETDNS_COMPILATION_COMMENT "${PACKAGE_NAME} ${GETDNS_VERSION} configured on ${CURRENT_DATE} for the ${API_VERSION} version of the API")

# Compiler flags
if (MSVC)
  # The Visual Studio C compiler is C90 with some of C99 and C11.
  # So full on warnings are not appropriate.
  add_compile_options(/W2)
else ()
  add_compile_options(-Wall -Wextra -Wpedantic)
endif ()

# Windows. Uh-oh.
set(getdns_system_libs "")
set(static_lib_suffix "")
if (DEFINED GETDNS_ON_WINDOWS)
  set(static_lib_suffix "_static")
  list(APPEND getdns_system_libs
    "ws2_32"
    "crypt32"
    "gdi32"
    "iphlpapi"
    "psapi"
    "userenv"
    )
endif ()

# Check for include files
check_include_file(assert.h HAVE_ASSERT_H)
check_include_file(inttypes.h HAVE_INTTYPES_H)
check_include_file(limits.h HAVE_LIMITS_H)
check_include_file(sys/limits.h HAVE_SYS_LIMITS_H)
check_include_file(stdarg.h HAVE_STDARG_H)
check_include_file(stdint.h HAVE_STDINT_H)
check_include_file(stdio.h HAVE_STDIO_H)
check_include_file(stdlib.h HAVE_STDLIB_H)
check_include_file(string.h HAVE_STRING_H)
check_include_file(strings.h HAVE_STRINGS_H)
check_include_file(time.h HAVE_TIME_H)
check_include_file(unistd.h HAVE_UNISTD_H)

check_include_file(fcntl.h HAVE_FCNTL_H)

check_include_file(signal.h HAVE_SIGNAL_H)
check_include_file(sys/poll.h HAVE_SYS_POLL_H)
check_include_file(poll.h HAVE_POLL_H)
check_include_file(resource.h HAVE_RESOURCE_H)
check_include_file(sys/types.h HAVE_SYS_TYPES_H)
check_include_file(sys/stat.h HAVE_SYS_STAT_H)

check_include_file(endian.h HAVE_ENDIAN_H)
check_include_file(netdb.h HAVE_NETDB_H)
check_include_file(arpa/inet.h HAVE_ARPA_INET_H)
check_include_file(netinet/in.h HAVE_NETINET_IN_H)
check_include_file(sys/select.h HAVE_SYS_SELECT_H)
check_include_file(sys/socket.h HAVE_SYS_SOCKET_H)
check_include_file(sys/sysctl.h HAVE_SYS_SYSCTL_H)
check_include_file(sys/time.h HAVE_SYS_TIME_H)
check_include_file(sys/wait.h HAVE_SYS_WAIT_H)

check_include_file(windows.h HAVE_WINDOWS_H)
check_include_file(winsock.h HAVE_WINSOCK_H)
check_include_file(winsock2.h HAVE_WINSOCK2_H)
check_include_file(ws2tcpip.h HAVE_WS2TCPIP_H)

# Check for include declarations
check_symbol_exists(getentropy unistd.h HAVE_DECL_GETENTROPY)
if (DEFINED GETDNS_ON_WINDOWS)
  set(CMAKE_REQUIRED_LIBRARIES ${getdns_system_libs})
  check_symbol_exists(inet_pton ws2tcpip.h HAVE_DECL_INET_PTON)
  check_symbol_exists(inet_ntop ws2tcpip.h HAVE_DECL_INET_NTOP)
else ()
  check_symbol_exists(inet_pton arpa/inet.h HAVE_DECL_INET_PTON)
  check_symbol_exists(inet_ntop arpa/inet.h HAVE_DECL_INET_NTOP)
endif ()
check_symbol_exists(mkstemp stdlib.h HAVE_DECL_MKSTEMP)
check_symbol_exists(sigemptyset signal.h HAVE_DECL_SIGEMPTYSET)
check_symbol_exists(sigfillset signal.h HAVE_DECL_SIGFILLSET)
check_symbol_exists(sigaddset signal.h HAVE_DECL_SIGADDSET)
check_symbol_exists(strptime time.h HAVE_DECL_STRPTIME)

# Check for functions
check_function_exists(fcntl HAVE_FCNTL)
check_function_exists(getauxval HAVE_GETAUXVAL)
check_function_exists(gettimeofday HAVE_GETTIMEOFDAY)
check_function_exists(ioctlsocket HAVE_IOCTLSOCKET)
check_function_exists(sigemptyset HAVE_SIGEMPTYSET)
check_function_exists(sigfillset HAVE_SIGFILLSET)
check_function_exists(sigaddset HAVE_SIGADDSET)
check_function_exists(strptime HAVE_STRPTIME)

# Check for types
check_type_size(sigset_t SIGSET_T)
check_type_size(_sigset_t _SIGSET_T)

# SSL library
find_package(OpenSSL "1.0.2" REQUIRED)

set(HAVE_SSL 1)
set(tlsdir "openssl")

set(CMAKE_REQUIRED_INCLUDES ${OPENSSL_INCLUDE_DIR})
check_include_file(openssl/ssl.h HAVE_OPENSSL_SSL_H)
check_include_file(openssl/evp.h HAVE_OPENSSL_EVP_H)
check_include_file(openssl/err.h HAVE_OPENSSL_ERR_H)
check_include_file(openssl/rand.h HAVE_OPENSSL_RAND_H)
check_include_file(openssl/conf.h HAVE_OPENSSL_CONF_H)
check_include_file(openssl/engine.h HAVE_OPENSSL_ENGINE_H)

set(CMAKE_REQUIRED_LIBRARIES ${OPENSSL_LIBRARIES})
check_function_exists(DSA_SIG_set0 HAVE_DSA_SIG_SET0)
check_function_exists(DSA_set0_pqg HAVE_DSA_SET0_PQG)
check_function_exists(DSA_set0_key HAVE_DSA_SET0_KEY)

check_function_exists(RSA_set0_key HAVE_RSA_SET0_KEY)

check_function_exists(EVP_md5 HAVE_EVP_MD5)
check_function_exists(EVP_sha1 HAVE_EVP_SHA1)
check_function_exists(EVP_sha224 HAVE_EVP_SHA224)
check_function_exists(EVP_sha256 HAVE_EVP_SHA256)
check_function_exists(EVP_sha384 HAVE_EVP_SHA384)
check_function_exists(EVP_sha512 HAVE_EVP_SHA512)

check_function_exists(EVP_dss1 HAVE_EVP_DSS1)
check_function_exists(EVP_DigestVerify HAVE_EVP_DIGESTVERIFY)

check_function_exists(EVP_MD_CTX_new HAVE_EVP_MD_CTX_NEW)

check_function_exists(HMAC_CTX_new HAVE_HMAC_CTX_NEW)

check_function_exists(OpenSSL_version_num HAVE_OPENSSL_VERSION_NUM)
check_function_exists(OpenSSL_version HAVE_OPENSSL_VERSION)

check_function_exists(SSL_CTX_dane_enable HAVE_SSL_CTX_DANE_ENABLE)

check_function_exists(SSL_CTX_set_ciphersuites HAVE_SSL_CTX_SET_CIPHERSUITES)
check_function_exists(SSL_set_ciphersuites HAVE_SSL_SET_CIPHERSUITES)

check_function_exists(OPENSSL_init_crypto HAVE_OPENSSL_INIT_CRYPTO)

check_symbol_exists(SSL_dane_enable "openssl/ssl.h" HAVE_SSL_DANE_ENABLE)
check_symbol_exists(SSL_CTX_set1_curves_list "openssl/ssl.h" HAVE_DECL_SSL_CTX_SET1_CURVES_LIST)
check_symbol_exists(SSL_set1_curves_list "openssl/ssl.h" HAVE_DECL_SSL_SET1_CURVES_LIST)
check_symbol_exists(SSL_set_min_proto_version "openssl/ssl.h" HAVE_DECL_SSL_SET_MIN_PROTO_VERSION)
check_symbol_exists(TLS_client_method "openssl/ssl.h" HAVE_TLS_CLIENT_METHOD)
check_symbol_exists(X509_get_notAfter "openssl/x509.h" HAVE_X509_GET_NOTAFTER)
check_symbol_exists(X509_get0_notAfter "openssl/x509.h" HAVE_X509_GET0_NOTAFTER)

check_symbol_exists(NID_ED25519 "openssl/obj_mac.h" HAVE_SSL_ED25519)
check_symbol_exists(NID_ED448 "openssl/obj_mac.h" HAVE_SSL_ED448)

# Threading library
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

if (CMAKE_USE_PTHREADS_INIT)
  set(HAVE_PTHREAD 1)
elseif (CMAKE_USE_WIN32_THREADS_INIT)
  set(HAVE_WINDOWS_THREADS 1)
else ()
  message(WARNING "Neither pthreads nor Windows threading available.")
endif ()

# Libidn2
if (USE_LIBIDN2)
  find_package(Libidn2 "2.0.0" REQUIRED)
  if (Libidn2_FOUND)
    set(HAVE_LIBIDN2 1)
  else()
      message(FATAL_ERROR "Libidn2 required but not found. Disable with USE_LIBIDN2 option.")
  endif()
endif()

# GnuTLS and Nettle. If using GnuTLS, we need the Nettle dev stuff to
# handle digital signature algorithms. GnuTLS uses Nettle internally.
if (USE_GNUTLS)
  find_package(GnuTLS "3.5.0" REQUIRED)
  find_package(Nettle "3.2" REQUIRED)

  set(tlsdir "gnutls")
  set(HAVE_NETTLE 1)

  set(CMAKE_REQUIRED_INCLUDES ${NETTLE_INCLUDE_DIR})
set(CMAKE_REQUIRED_LIBRARIES ${NETTLE_LIBRARIES})
  check_include_file(nettle/dsa-compat.h HAVE_NETTLE_DSA_COMPAT_H)
  check_include_file(nettle/eddsa.h HAVE_NETTLE_EDDSA_H)

  # API change in Nettle 3.4.
  check_symbol_exists(nettle_get_secp_256r1 "nettle/ecc-curve.h" HAVE_NETTLE_GET_SECP_256R1)
  check_symbol_exists(nettle_get_secp_384r1 "nettle/ecc-curve.h" HAVE_NETTLE_GET_SECP_384R1)
endif()

# Sort out what signature algorithms can be used.
if (USE_ED25519)
  if (USE_GNUTLS)
    if (NOT HAVE_NETTLE_EDDSA_H)
      message(WARNING "ED25519 enabled and Nettle does not support it. Disabled.")
      unset(USE_ED25519)
    endif ()
  elseif (NOT HAVE_SSL_ED25519)
    message(WARNING "ED25519 enabled and OpenSSL does not support it. Disabled.")
    unset(USE_ED25519)
  endif ()
endif ()

if (USE_ED448)
  if (USE_GNUTLS)
    message(WARNING "ED448 enabled and Nettle support not implemented. Disabled.")
    unset(USE_ED448)
  elseif (NOT HAVE_SSL_ED448)
    message(WARNING "ED448 enabled and OpenSSL does not support it. Disabled.")
    unset(USE_ED448)
  endif ()
endif ()

# Stuff that might be in a BSD library
check_symbol_exists(strlcpy string.h HAVE_DECL_STRLCPY)
check_symbol_exists(arc4random stdlib.h HAVE_DECL_ARC4RANDOM)
check_symbol_exists(arc4random_uniform stdlib.h HAVE_DECL_ARC4RANDOM_UNIFORM)

check_function_exists(strlcpy HAVE_STRLCPY)
check_function_exists(arc4random HAVE_ARC4RANDOM)
check_function_exists(arc4random_uniform HAVE_ARC4RANDOM_UNIFORM)

if (NOT
    (HAVE_STRLCPY AND HAVE_DECL_STRLCPY AND
     HAVE_ARC4RANDOM AND HAVE_DECL_ARC4RANDOM AND
     HAVE_ARC4RANDOM_UNIFORM AND HAVE_DECL_ARC4RANDOM_UNIFORM))
 find_library(BSD_LIBRARY bsd)
 if (BSD_LIBRARY)
   unset(CMAKE_REQUIRED_LIBRARIES)
   set(CMAKE_REQUIRED_LIBRARIES ${BSD_LIBRARY})
   mark_as_advanced(BSD_LIBRARY)
   list(APPEND getdns_system_libs ${BSD_LIBRARY})

   check_include_file(bsd/stdlib.h HAVE_BSD_STDLIB_H)
   check_include_file(bsd/string.h HAVE_BSD_STRING_H)

   check_symbol_exists(strlcpy "bsd/string.h" HAVE_BSD_DECL_STRLCPY)
   set(HAVE_DECL_STRLCPY ${HAVE_BSD_DECL_STRLCPY})
   check_symbol_exists(arc4random "bsd/stdlib.h" HAVE_BSD_DECL_ARC4RANDOM)
   set(HAVE_DECL_ARC4RANDOM ${HAVE_BSD_DECL_ARC4RANDOM})
   check_symbol_exists(arc4random_uniform "bsd/stdlib.h" HAVE_BSD_DECL_ARC4RANDOM_UNIFORM)
   set(HAVE_DECL_ARC4RANDOM_UNIFORM ${HAVE_BSD_DECL_ARC4RANDOM_UNIFORM})

   check_function_exists(strlcpy HAVE_BSD_STRLCPY)
   set(HAVE_STRLCPY ${HAVE_BSD_STRLCPY})
   check_function_exists(arc4random HAVE_BSD_ARC4RANDOM)
   set(HAVE_ARC4RANDOM ${HAVE_BSD_ARC4RANDOM})
   check_function_exists(arc4random_uniform HAVE_BSD_ARC4RANDOM_UNIFORM)
   set(HAVE_ARC4RANDOM_UNIFORM ${HAVE_BSD_ARC4RANDOM_UNIFORM})
 endif ()
endif ()
mark_as_advanced(BSD_LIBRARY)

# If we're not stub only, we need libunbound.
if (NOT ENABLE_STUB_ONLY)
  find_package(Libunbound "1.5.9" REQUIRED)
  set(HAVE_LIBUNBOUND 1)
  list(APPEND getdns_system_libs Libunbound::Libunbound)

  set(CMAKE_REQUIRED_INCLUDES ${LIBUNBOUND_INCLUDE_DIR})
  set(CMAKE_REQUIRED_LIBRARIES ${LIBUNBOUND_LIBRARIES})
  check_include_file(unbound-event.h HAVE_UNBOUND_EVENT_H)
  check_symbol_exists(ub_ctx_create_ub_event "unbound-event.h" HAVE_UNBOUND_EVENT_API)
  check_symbol_exists(ub_ctx_set_stub "unbound-event.h" HAVE_UB_CTX_SET_STUB)
endif ()

# Event loop extension
set(DEFAULT_EVENTLOOP "select_eventloop")
if (ENABLE_POLL_EVENTLOOP)
  if (HAVE_SYS_POLL_H)
    set(TEST_CFLAG "-DHAVE_SYS_POLL_H=1")
  endif ()
  try_compile(USE_POLL_DEFAULT_EVENTLOOP
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/tests/test_poll.c
    COMPILE_DEFINITIONS "${TEST_CFLAG}"
    )
  if (USE_POLL_DEFAULT_EVENTLOOP)
    set(DEFAULT_EVENTLOOP "poll_eventloop")
  endif ()
endif ()

# Custom checks
set(STRPTIME_TEST_SOURCE "\n
  #define _XOPEN_SOURCE 600\n
  #include <time.h>\n
  int main(void) { struct tm tm; char *res;\n
  res = strptime(\"2010-07-15T00:00:00+00:00\", \"%t%Y%t-%t%m%t-%t%d%tT%t%H%t:%t%M%t:%t%S%t\", &tm);\n
  if (!res) return 2;\n
  res = strptime(\"20070207111842\", \"%Y%m%d%H%M%S\", &tm);\n
  if (!res) return 1; return 0; }")

if (HAVE_STRPTIME)
  check_c_source_runs("${STRPTIME_TEST_SOURCE}" STRPTIME_WORKS)
endif ()

try_compile(HAVE___FUNC__
  ${CMAKE_CURRENT_BINARY_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/cmake/tests/test___func__.c
)

# TCP Fast Open.
if (NOT ENABLE_TCP_FAST_OPEN)
  message(WARNING "TCP Fast Open disabled.")
else ()
  if (APPLE AND HAVE_SYS_SOCKET_H)
    check_symbol_exists(CONNECT_RESUME_ON_READ_WRITE "sys/socket.h" USE_OSX_TCP_FASTOPEN)
    if (NOT USE_OSX_TCP_FASTOPEN)
      message(WARNING "TCP Fast Open not available, continuing without.")
    endif ()
  else ()
    check_include_file(netinet/tcp.h HAVE_NETINET_TCP_H)
    check_symbol_exists(TCP_FASTOPEN "sys/socket.h;netinet/tcp.h" HAVE_DECL_TCP_FASTOPEN)
    check_symbol_exists(TCP_FASTOPEN_CONNECT "sys/socket.h;netinet/tcp.h" HAVE_DECL_TCP_FASTOPEN_CONNECT)
    check_symbol_exists(MSG_FASTOPEN "sys/socket.h;netinet/tcp.h" HAVE_DECL_MSG_FASTOPEN)
    if (NOT HAVE_DECL_TCP_FASTOPEN)
      message(WARNING "TCP Fast Open not available, continuing without.")
    endif ()
  endif ()
endif ()

# Main library
add_library(getdns_objects OBJECT
  src/anchor.c
  src/const-info.c
  src/convert.c
  src/context.c
  src/dict.c
  src/dnssec.c
  src/general.c
  src/list.c
  src/request-internal.c
  src/mdns.c
  src/platform.c
  src/pubkey-pinning.c
  src/rr-dict.c
  src/rr-iter.c
  src/server.c
  src/stub.c
  src/sync.c
  src/ub_loop.c
  src/util-internal.c

  src/extension/${DEFAULT_EVENTLOOP}.c

  src/gldns/keyraw.c
  src/gldns/gbuffer.c
  src/gldns/wire2str.c
  src/gldns/parse.c
  src/gldns/parseutil.c
  src/gldns/rrdef.c
  src/gldns/str2wire.c

  src/util/rbtree.c
  src/util/lruhash.c
  src/util/lookup3.c
  src/util/locks.c

  src/jsmn/jsmn.c

  src/yxml/yxml.c

  src/tls/val_secalgo.c
  src/tls/anchor-internal.c

  src/${tlsdir}/tls.c
  src/${tlsdir}/pubkey-pinning-internal.c
  src/${tlsdir}/keyraw-internal.c

  ${CMAKE_CURRENT_BINARY_DIR}/version.c
  )
if (NOT HAVE_GETTIMEOFDAY)
  target_sources(getdns_objects PRIVATE src/compat/gettimeofday.c)
endif ()
if (NOT HAVE_DECL_INET_PTON)
  target_sources(getdns_objects PRIVATE src/compat/inet_pton.c)
endif ()
if (NOT HAVE_DECL_INET_NTOP)
  target_sources(getdns_objects PRIVATE src/compat/inet_ntop.c)
endif ()
if (NOT HAVE_DECL_MKSTEMP)
  target_sources(getdns_objects PRIVATE src/compat/mkstemp.c)
endif ()
if (NOT HAVE_DECL_STRLCPY)
  target_sources(getdns_objects PRIVATE src/compat/strlcpy.c)
endif ()
if (NOT HAVE_DECL_ARC4RANDOM)
  target_sources(getdns_objects PRIVATE
    src/compat/arc4random.c
    src/compat/explicit_bzero.c
    src/compat/arc4_lock.c
    )

  if (NOT HAVE_DECL_GETENTROPY)
    if (DEFINED GETDNS_ON_WINDOWS)
      target_sources(getdns_objects PRIVATE src/compat/getentropy_win.c)
    elseif (APPLE)
      target_sources(getdns_objects PRIVATE src/compat/getentropy_osx.c)
    elseif (DEFINED LINUX)
      target_sources(getdns_objects PRIVATE src/compat/getentropy_linux.c)
    endif ()
  endif ()
endif ()
if (NOT HAVE_DECL_ARC4RANDOM_UNIFORM)
  target_sources(getdns_objects PRIVATE src/compat/arc4random_uniform.c)
endif ()
if (NOT STRPTIME_WORKS)
  target_sources(getdns_objects PRIVATE src/compat/strptime.c)
endif ()
target_include_directories(getdns_objects
  PUBLIC
  src

  PRIVATE
  src/util/auxiliary
  src/${tlsdir}
  src/tls
  src/yxml

  ${CMAKE_CURRENT_BINARY_DIR}

  # Note - CMake 3.5 doesn't like target_link_libraries on objects,
  # which would be preferred way to add an include dependency.
  ${OPENSSL_INCLUDE_DIR}
  )
target_compile_definitions(getdns_objects PRIVATE JSMN_GETDNS YXML_GETDNS)
if (NOT HAVE_SSL_DANE_ENABLE)
  target_sources(getdns_objects PRIVATE src/ssl_dane/danessl.c)
  target_include_directories(getdns_objects PRIVATE src/ssl_dane)
  set(USE_DANESSL 1)
endif ()
if (Libidn_FOUND)
  target_include_directories(getdns_objects PRIVATE ${LIBIDN_INCLUDE_DIR})
endif ()
if (Libidn2_FOUND)
  target_include_directories(getdns_objects PRIVATE ${LIBIDN2_INCLUDE_DIR})
endif ()
if (Libunbound_FOUND)
  target_include_directories(getdns_objects PRIVATE ${LIBUNBOUND_INCLUDE_DIR})
endif ()
if (GnuTLS_FOUND)
  target_include_directories(getdns_objects PRIVATE ${GNUTLS_INCLUDE_DIR})
endif ()

# Don't compile separate objects for shared and static libraries.
# Yes, -fPIC is slightly suboptimal for static libraries, but it looks
# to me that it's the behaviour the autoconf build follows.
set_property(TARGET getdns_objects PROPERTY POSITION_INDEPENDENT_CODE 1)
set_property(TARGET getdns_objects PROPERTY C_STANDARD 11)

# Static library version of main library.
if (ENABLE_STATIC)
  add_library(getdns STATIC $<TARGET_OBJECTS:getdns_objects>)
  target_include_directories(getdns PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>"
    ${CMAKE_CURRENT_BINARY_DIR}
    )
  target_link_libraries(getdns
    PUBLIC
    OpenSSL::SSL
    OpenSSL::Crypto
    Threads::Threads
    ${getdns_system_libs}
    )
  if (Libunbound_FOUND)
    target_link_libraries(getdns PUBLIC Libunbound::Libunbound)
  endif ()
  if (Libidn_FOUND)
    target_link_libraries(getdns PUBLIC Libidn::Libidn)
  endif ()
  if (Libidn2_FOUND)
    target_link_libraries(getdns PUBLIC Libidn2::Libidn2)
  endif ()
  if (GnuTLS_FOUND)
    target_link_libraries(getdns PUBLIC GnuTLS::GnuTLS GnuTLS::Dane)
  endif ()
  if (Nettle_FOUND)
    target_link_libraries(getdns PUBLIC Nettle::Nettle Nettle::Hogweed)
  endif ()
  set_target_properties(getdns PROPERTIES OUTPUT_NAME getdns${static_lib_suffix})
endif ()

# Shared library version of main library.
if (ENABLE_SHARED)
  add_library(getdns_shared SHARED $<TARGET_OBJECTS:getdns_objects>)
  target_include_directories(getdns_shared PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>"
    ${CMAKE_CURRENT_BINARY_DIR}
    )
  target_link_libraries(getdns_shared
    PUBLIC
    OpenSSL::SSL
    OpenSSL::Crypto
    Threads::Threads
    ${getdns_system_libs}
    )
  if (Libunbound_FOUND)
    target_link_libraries(getdns_shared PUBLIC Libunbound::Libunbound)
  endif ()
  if (Libidn_FOUND)
    target_link_libraries(getdns_shared PUBLIC Libidn::Libidn)
  endif ()
  if (Libidn2_FOUND)
    target_link_libraries(getdns_shared PUBLIC Libidn2::Libidn2)
  endif ()
  if (GnuTLS_FOUND)
    target_link_libraries(getdns_shared PUBLIC GnuTLS::GnuTLS GnuTLS::Dane)
  endif ()
  if (Nettle_FOUND)
    target_link_libraries(getdns_shared PUBLIC Nettle::Nettle Nettle::Hogweed)
  endif ()
  set_target_properties(getdns_shared PROPERTIES OUTPUT_NAME getdns)
  target_shared_library_version(getdns_shared ${GETDNS_VERSION_CURRENT} ${GETDNS_VERSION_REVISION} ${GETDNS_VERSION_AGE})

  # Generate platform-specific link file with the export symbols.
  file(STRINGS src/libgetdns.symbols symbols)
  target_shared_library_exports(getdns_shared getdns "${symbols}")

  # If we're not building a static library, use this wherever we use
  # the static library in tool and test builds.
  if (NOT ENABLE_STATIC)
    add_library(getdns ALIAS getdns_shared)
  endif ()
endif ()

# libev extension.
if (USE_LIBEV)
  find_package(Libev)
  if (Libev_FOUND)
    # Copy module header to getdns include dir.
    file(COPY src/getdns/getdns_ext_libev.h DESTINATION getdns)

    add_library(ev_objects OBJECT src/extension/libev.c)
    target_include_directories(ev_objects
      PRIVATE
      src
      ${CMAKE_CURRENT_BINARY_DIR}
      ${LIBEV_INCLUDE_DIR}
      )
    set_property(TARGET ev_objects PROPERTY POSITION_INDEPENDENT_CODE 1)
    set_property(TARGET ev_objects PROPERTY C_STANDARD 11)
    if (ENABLE_STATIC)
      add_library(getdns_ex_ev STATIC $<TARGET_OBJECTS:ev_objects>)
      target_include_directories(getdns_ex_ev PRIVATE Libev::Libev)
      target_link_libraries(getdns_ex_ev PUBLIC getdns Libev::Libev)
      if (Libunbound_FOUND)
        target_link_libraries(getdns_ex_ev PUBLIC Libunbound::Libunbound)
      endif ()
      set_target_properties(getdns_ex_ev PROPERTIES OUTPUT_NAME getdns_ex_ev${static_lib_suffix})
    endif ()
    if (ENABLE_SHARED)
      add_library(getdns_ex_ev_shared SHARED $<TARGET_OBJECTS:ev_objects>)
      target_include_directories(getdns_ex_ev_shared PRIVATE Libev::Libev)
      target_link_libraries(getdns_ex_ev_shared PUBLIC getdns_shared Libev::Libev)
      if (Libunbound_FOUND)
        target_link_libraries(getdns_ex_ev_shared PUBLIC Libunbound::Libunbound)
      endif ()
      set_target_properties(getdns_ex_ev_shared PROPERTIES OUTPUT_NAME getdns_ex_ev)
      target_shared_library_version(getdns_ex_ev_shared ${GETDNS_VERSION_CURRENT} ${GETDNS_VERSION_REVISION} ${GETDNS_VERSION_AGE})
      file(STRINGS src/extension/libev.symbols symbols)
      target_shared_library_exports(getdns_ex_ev_shared getdns_ex_ev "${symbols}")
      if (NOT ENABLE_STATIC)
        add_library(getdns_ex_ev ALIAS getdns_ex_ev_shared)
      endif ()
    endif ()
  else ()
    message(WARNING "Libev support library build requested, but libev not found. Disabled.")
    unset(USE_LIBEV)
  endif ()
endif ()

# libevent2 extension.
if (USE_LIBEVENT2)
  find_package(Libevent2)
  if (Libevent2_FOUND)
    # Given libevent2, set defines required by source.
    set(HAVE_EVENT2_EVENT_H 1)
    set(HAVE_EVENT_BASE_FREE 1)
    set(HAVE_EVENT_BASE_NEW 1)

    # Copy module header to getdns include dir.
    file(COPY src/getdns/getdns_ext_libevent.h DESTINATION getdns)

    add_library(event2_objects OBJECT src/extension/libevent.c)
    target_include_directories(event2_objects
      PRIVATE
      src
      ${CMAKE_CURRENT_BINARY_DIR}
      ${LIBEVENT2_INCLUDE_DIR}
      )
    set_property(TARGET event2_objects PROPERTY POSITION_INDEPENDENT_CODE 1)
    set_property(TARGET event2_objects PROPERTY C_STANDARD 11)
    if (ENABLE_STATIC)
      add_library(getdns_ex_event STATIC $<TARGET_OBJECTS:event2_objects>)
      target_include_directories(getdns_ex_event PRIVATE Libevent2::Libevent_code)
      target_link_libraries(getdns_ex_event PUBLIC getdns Libevent2::Libevent_core)
      if (Libunbound_FOUND)
        target_link_libraries(getdns_ex_event PUBLIC Libunbound::Libunbound)
      endif ()
      set_target_properties(getdns_ex_event PROPERTIES OUTPUT_NAME getdns_ex_event${static_lib_suffix})
    endif ()
    if (ENABLE_SHARED)
      add_library(getdns_ex_event_shared SHARED $<TARGET_OBJECTS:event2_objects>)
      target_include_directories(getdns_ex_event_shared PRIVATE Libevent2::Libevent_code)
      target_link_libraries(getdns_ex_event_shared PUBLIC getdns_shared Libevent2::Libevent_core)
      if (Libunbound_FOUND)
        target_link_libraries(getdns_ex_event_shared PUBLIC Libunbound::Libunbound)
      endif ()
      set_target_properties(getdns_ex_event_shared PROPERTIES OUTPUT_NAME getdns_ex_event)
      target_shared_library_version(getdns_ex_event_shared ${GETDNS_VERSION_CURRENT} ${GETDNS_VERSION_REVISION} ${GETDNS_VERSION_AGE})
      file(STRINGS src/extension/libevent.symbols symbols)
      target_shared_library_exports(getdns_ex_event_shared getdns_ex_event "${symbols}")
      if (NOT ENABLE_STATIC)
        add_library(getdns_ex_event ALIAS getdns_ex_event_shared)
      endif ()
    endif ()
  else ()
    message(WARNING "Libevent2 support library build requested, but libevent2 not found. Disabled.")
    unset(USE_LIBEVENT2)
  endif ()
endif ()

# libuv extension.
if (USE_LIBUV)
  find_package(Libuv)
  if (Libuv_FOUND)
    # Check for new-style callbacks.
    try_compile(HAVE_NEW_UV_TIMER_CB
      ${CMAKE_CURRENT_BINARY_DIR}
      ${CMAKE_CURRENT_SOURCE_DIR}/cmake/tests/test_uv_cb.c
    )

    # Copy module header to getdns include dir.
    file(COPY src/getdns/getdns_ext_libuv.h DESTINATION getdns)

    add_library(uv_objects OBJECT src/extension/libuv.c)
    target_include_directories(uv_objects
      PRIVATE
      src
      ${CMAKE_CURRENT_BINARY_DIR}
      ${LIBUV_INCLUDE_DIR}
      )
    set_property(TARGET uv_objects PROPERTY POSITION_INDEPENDENT_CODE 1)
    set_property(TARGET uv_objects PROPERTY C_STANDARD 11)
    if (ENABLE_STATIC)
      add_library(getdns_ex_uv STATIC $<TARGET_OBJECTS:uv_objects>)
      target_include_directories(getdns_ex_uv PRIVATE Libuv::Libuv)
      target_link_libraries(getdns_ex_uv PUBLIC getdns Libuv::Libuv)
      if (Libunbound_FOUND)
        target_link_libraries(getdns_ex_uv PUBLIC Libunbound::Libunbound)
      endif ()
      set_target_properties(getdns_ex_uv PROPERTIES OUTPUT_NAME getdns_ex_uv${static_lib_suffix})
    endif ()
    if (ENABLE_SHARED)
      add_library(getdns_ex_uv_shared SHARED $<TARGET_OBJECTS:uv_objects>)
      target_include_directories(getdns_ex_uv_shared PRIVATE Libuv::Libuv)
      target_link_libraries(getdns_ex_uv_shared PUBLIC getdns_shared Libuv::Libuv)
      if (Libunbound_FOUND)
        target_link_libraries(getdns_ex_uv_shared PUBLIC Libunbound::Libunbound)
      endif ()
      set_target_properties(getdns_ex_uv_shared PROPERTIES OUTPUT_NAME getdns_ex_uv)
      target_shared_library_version(getdns_ex_uv_shared ${GETDNS_VERSION_CURRENT} ${GETDNS_VERSION_REVISION} ${GETDNS_VERSION_AGE})
      file(STRINGS src/extension/libuv.symbols symbols)
      target_shared_library_exports(getdns_ex_uv_shared getdns_ex_uv "${symbols}")
      if (NOT ENABLE_STATIC)
        add_library(getdns_ex_uv ALIAS getdns_ex_uv_shared)
      endif ()
    endif ()
  else ()
    message(WARNING "Libuv support library build requested, but libuv not found. Disabled.")
    unset(USE_LIBUV)
  endif ()
endif ()

# The tools.
if (BUILD_GETDNS_QUERY)
  add_executable(getdns_query src/tools/getdns_query.c)
  if (NOT HAVE_GETTIMEOFDAY)
    target_sources(getdns_query PRIVATE src/compat/gettimeofday.c)
  endif ()
  target_link_libraries(getdns_query PRIVATE getdns)
  if (Libunbound_FOUND)
    target_link_libraries(getdns_query PUBLIC Libunbound::Libunbound)
  endif ()
  set_property(TARGET getdns_query PROPERTY C_STANDARD 11)
endif ()

if (BUILD_GETDNS_SERVER_MON)
  add_executable(getdns_server_mon src/tools/getdns_server_mon.c)
  target_link_libraries(getdns_server_mon
    PUBLIC
    OpenSSL::SSL
    OpenSSL::Crypto
    PRIVATE
    getdns
    )
  if (Libunbound_FOUND)
    target_link_libraries(getdns_server_mon PUBLIC Libunbound::Libunbound)
  endif ()
  set_property(TARGET getdns_server_mon PROPERTY C_STANDARD 11)
endif ()

if (BUILD_TESTING)
  if (WIN32)
    message(WARNING "Test programs require libcheck and so do not build on Windows, skipping.")
  else ()
    find_package(Check "0.9.6" REQUIRED)
    set(GETDNS_TEST_SOURCE_FILES 
      src/test/check_getdns_common.c
      src/test/check_getdns_context_set_timeout.c
      src/test/check_getdns_transport.c
      src/test/check_getdns_selectloop.c
      src/test/check_getdns.c)
    set(GETDNS_TEST_LINK_LIBS
      getdns
      Check::Check
      Threads::Threads)

    add_executable(check_getdns ${GETDNS_TEST_SOURCE_FILES})
    target_link_libraries(check_getdns PRIVATE ${GETDNS_TEST_LINK_LIBS})
    add_test(NAME test_noeventloop COMMAND check_getdns)
    set_property(TEST test_noeventloop PROPERTY
      ENVIRONMENT "GETDNS_TEST_PORT=43210;CK_TIMEOUT_MULTIPLIER=2;CK_LOG_FILE_NAME=check_getdns.log"
    )

    if (USE_LIBEVENT2)
      add_executable(check_getdns_event ${GETDNS_TEST_SOURCE_FILES})
      target_link_libraries(check_getdns_event PRIVATE ${GETDNS_TEST_LINK_LIBS} getdns_ex_event)
      add_test(NAME test_libevent COMMAND check_getdns_event)
      set_property(TEST test_libevent PROPERTY
        ENVIRONMENT "GETDNS_TEST_PORT=44321;CK_TIMEOUT_MULTIPLIER=2;CK_LOG_FILE_NAME=check_getdns_event.log"
      )
    endif()

    if (USE_LIBEV)
      add_executable(check_getdns_ev ${GETDNS_TEST_SOURCE_FILES})
      target_link_libraries(check_getdns_ev PRIVATE ${GETDNS_TEST_LINK_LIBS} getdns_ex_ev)
      add_test(NAME test_libev COMMAND check_getdns_ev)
      set_property(TEST test_libev PROPERTY
        ENVIRONMENT "GETDNS_TEST_PORT=45321;CK_TIMEOUT_MULTIPLIER=2;CK_LOG_FILE_NAME=check_getdns_ev.log"
      )
    endif()

    if (USE_LIBUV)
      add_executable(check_getdns_uv ${GETDNS_TEST_SOURCE_FILES})
      target_link_libraries(check_getdns_uv PRIVATE ${GETDNS_TEST_LINK_LIBS} getdns_ex_uv)
      add_test(NAME test_libuv COMMAND check_getdns_uv)
      set_property(TEST test_libuv PROPERTY
        ENVIRONMENT "GETDNS_TEST_PORT=46321;CK_TIMEOUT_MULTIPLIER=2;CK_LOG_FILE_NAME=check_getdns_uv.log"
      )
    endif()

    add_executable(tests_dict
      src/test/tests_dict.c
      src/test/testmessages.c
      )
    target_link_libraries(tests_dict PRIVATE getdns Check::Check)

    add_executable(tests_list
      src/test/tests_list.c
      src/test/testmessages.c
      )
    target_link_libraries(tests_list PRIVATE getdns Check::Check)

    add_executable(tests_namespaces src/test/tests_namespaces.c)
    target_link_libraries(tests_namespaces PRIVATE getdns Check::Check)

    add_executable(tests_stub_async
      src/test/tests_stub_async.c
      src/test/testmessages.c
      )
    target_link_libraries(tests_stub_async PRIVATE getdns Check::Check)

    add_executable(tests_stub_sync src/test/tests_stub_sync.c)
    target_link_libraries(tests_stub_sync PRIVATE getdns Check::Check)
  endif()
endif ()

# Substitutions in files.
configure_file(cmake/include/cmakeconfig.h.in config.h)
configure_file(src/getdns/getdns.h.in getdns/getdns.h)
configure_file(src/getdns/getdns_extra.h.in getdns/getdns_extra.h)
configure_file(src/version.c.in version.c)

set(version ${PACKAGE_VERSION})
set(date ${API_VERSION})
file(GLOB mans doc/*.3.in)
file(MAKE_DIRECTORY man3)
foreach (man ${mans})
  get_filename_component(out ${man} NAME_WE)
  configure_file(${man} man3/${out}.3 @ONLY)

  # Look through the page and make copies of the page for all APIs
  # defined in that page. Defined means listed in a line ".B <name>"
  # between lines ".SH NAME" and ".SH LIBRARY". Ignore terminating ","
  # or spaces in .B line.
  file(STRINGS ${man} manpage REGEX "^\\.(SH +NAME|SH +LIBRARY|B )")
  set(in_list 0)
  foreach (line ${manpage})
    if ("${line}" MATCHES "^\\.SH +NAME")
      set(in_list 1)
    elseif ("${line}" MATCHES "^\\.SH +LIBRARY")
      set(in_list 0)
    elseif (${in_list})
      string(REGEX REPLACE ".B +([^ ,]+).*" "\\1" alt "${line}")
      configure_file(${man} man3/${alt}.3 @ONLY)
    endif ()
  endforeach()
endforeach()

set(prefix ${CMAKE_INSTALL_PREFIX})
configure_file(getdns.pc.in getdns.pc @ONLY)

# Installing.
if (ENABLE_STATIC)
  install(TARGETS getdns LIBRARY DESTINATION lib ARCHIVE DESTINATION lib)
  if (USE_LIBEV)
    install(TARGETS getdns_ex_ev LIBRARY DESTINATION lib ARCHIVE DESTINATION lib)
  endif ()
  if (USE_LIBEVENT2)
    install(TARGETS getdns_ex_event LIBRARY DESTINATION lib ARCHIVE DESTINATION lib)
  endif ()
  if (USE_LIBUV)
    install(TARGETS getdns_ex_uv LIBRARY DESTINATION lib ARCHIVE DESTINATION lib)
  endif ()
endif ()
if (ENABLE_SHARED)
  install(TARGETS getdns_shared LIBRARY DESTINATION lib ARCHIVE DESTINATION lib)
  if (USE_LIBEV)
    install(TARGETS getdns_ex_ev_shared LIBRARY DESTINATION lib ARCHIVE DESTINATION lib)
  endif ()
  if (USE_LIBEVENT2)
    install(TARGETS getdns_ex_event_shared LIBRARY DESTINATION lib ARCHIVE DESTINATION lib)
  endif ()
  if (USE_LIBUV)
    install(TARGETS getdns_ex_uv_shared LIBRARY DESTINATION lib ARCHIVE DESTINATION lib)
  endif ()
endif ()
if (BUILD_GETDNS_QUERY)
  install(TARGETS getdns_query RUNTIME DESTINATION bin)
endif ()
if (BUILD_GETDNS_SERVER_MON)
  install(TARGETS getdns_server_mon RUNTIME DESTINATION bin)
endif ()

install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/getdns DESTINATION include)
install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/man3 DESTINATION share/man)

set(docdir share/doc/getdns)
install(FILES AUTHORS ChangeLog COPYING LICENSE NEWS README.md DESTINATION ${docdir})
install(FILES spec/index.html DESTINATION ${docdir}/spec)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/getdns.pc DESTINATION lib/pkgconfig)

install(CODE "message(\"\
***\n\
***  !!! IMPORTANT !!!!\n\
***\n\
***  From release 1.2.0, getdns comes with built-in DNSSEC\n\
***  trust anchor management.  External trust anchor management,\n\
***  for example with unbound-anchor, is no longer necessary\n\
***  and no longer recommended.\n\
***\n\
***  Previously installed trust anchors, in the default location -\n\
***\n\
***        /etc/unbound/getdns-root.key\n\
***\n\
***  - will be preferred and used for DNSSEC validation, however\n\
***  getdns will fallback to trust-anchors obtained via built-in\n\
***  trust anchor management when the anchors from the default\n\
***  location fail to validate the root DNSKEY rrset.\n\
***\n\
***  To prevent expired DNSSEC trust anchors to be used for\n\
***  validation, we strongly recommend removing the trust anchors\n\
***  on the default location when there is no active external\n\
***  trust anchor management keeping it up-to-date.\n\
***\")")

if (BUILD_STUBBY)
  add_subdirectory(stubby)
endif ()

if (BUILD_EXAMPLES)
  add_executable(example-all-functions spec/example/example-all-functions.c)
  target_include_directories(example-all-functions PRIVATE spec/example)
  target_link_libraries(example-all-functions PRIVATE getdns)
  set_property(TARGET example-all-functions PROPERTY C_STANDARD 11)

  add_executable(example-synchronous spec/example/example-synchronous.c)
  target_include_directories(example-synchronous PRIVATE spec/example)
  target_link_libraries(example-synchronous PRIVATE getdns)
  set_property(TARGET example-synchronous PROPERTY C_STANDARD 11)

  if (USE_LIBEVENT2)
    add_executable(example-simple-answers spec/example/example-simple-answers.c)
    target_include_directories(example-simple-answers PRIVATE spec/example)
    target_link_libraries(example-simple-answers PRIVATE getdns getdns_ex_event)
    set_property(TARGET example-simple-answers PROPERTY C_STANDARD 11)

    add_executable(example-tree spec/example/example-tree.c)
    target_include_directories(example-tree PRIVATE spec/example)
    target_link_libraries(example-tree PRIVATE getdns getdns_ex_event)
    set_property(TARGET example-tree PROPERTY C_STANDARD 11)

    add_executable(example-reverse spec/example/example-reverse.c)
    target_include_directories(example-reverse PRIVATE spec/example)
    target_link_libraries(example-reverse PRIVATE getdns getdns_ex_event)
    set_property(TARGET example-reverse PROPERTY C_STANDARD 11)
  else ()
    message(WARNING "\
Three examples from the specification need libevent. \
libevent was not found or usable at compile time. \
To compile and run all examples from the spec, make sure \
libevent is available and usable during configuration.")
  endif ()
endif ()

if (BUILD_DOXYGEN)
  find_package(Doxygen REQUIRED)

  set(srcdir ${CMAKE_CURRENT_SOURCE_DIR}/src)
  configure_file(src/Doxyfile.in Doxyfile @ONLY)
  add_custom_command(OUTPUT doc/html/index.html
    COMMAND ${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile
    MAIN_DEPENDENCY Doxyfile
    COMMENT "Generating Doxygen docs.")
  add_custom_target(doc ALL DEPENDS doc/html/index.html)
endif ()
