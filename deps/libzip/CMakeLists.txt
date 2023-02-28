cmake_minimum_required(VERSION 3.0.2)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
if (${CMAKE_VERSION} VERSION_LESS "3.17.0")
  list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake-compat)
endif()

project(libzip
  VERSION 1.9.2
  LANGUAGES C)

option(ENABLE_COMMONCRYPTO "Enable use of CommonCrypto" ON)
option(ENABLE_GNUTLS "Enable use of GnuTLS" ON)
option(ENABLE_MBEDTLS "Enable use of mbed TLS" ON)
option(ENABLE_OPENSSL "Enable use of OpenSSL" ON)
option(ENABLE_WINDOWS_CRYPTO "Enable use of Windows cryptography libraries" ON)

option(ENABLE_BZIP2 "Enable use of BZip2" ON)
option(ENABLE_LZMA "Enable use of LZMA" ON)
option(ENABLE_ZSTD "Enable use of Zstandard" ON)

option(ENABLE_FDOPEN "Enable zip_fdopen, which is not allowed in Microsoft CRT secure libraries" ON)

option(BUILD_TOOLS "Build tools in the src directory (zipcmp, zipmerge, ziptool)" ON)
option(BUILD_REGRESS "Build regression tests" ON)
option(BUILD_EXAMPLES "Build examples" ON)
option(BUILD_DOC "Build documentation" ON)

include(CheckFunctionExists)
include(CheckIncludeFiles)
include(CheckLibraryExists)
include(CheckSymbolExists)
include(CheckTypeSize)
include(CheckCSourceRuns)
include(CheckCSourceCompiles)
include(CheckStructHasMember)
include(TestBigEndian)
include(GNUInstallDirs)

if(ENABLE_COMMONCRYPTO)
  check_include_files(CommonCrypto/CommonCrypto.h COMMONCRYPTO_FOUND)
endif()
if(ENABLE_GNUTLS)
  find_package(Nettle 3.0)
  find_package(GnuTLS)
endif()
if(ENABLE_MBEDTLS)
  find_package(MbedTLS 1.0)
endif()
if(ENABLE_OPENSSL)
  find_package(OpenSSL)
endif()
if(WIN32)
  if(ENABLE_WINDOWS_CRYPTO)
    set(WINDOWS_CRYPTO_FOUND TRUE)
  endif()
endif()

option(BUILD_SHARED_LIBS "Build shared libraries" ON)
option(LIBZIP_DO_INSTALL "Install libzip and the related files" ON)

option(SHARED_LIB_VERSIONNING "Add SO version in .so build" ON)

find_program(MDOCTOOL NAMES mandoc groff)
if (MDOCTOOL)
  set(DOCUMENTATION_FORMAT "mdoc" CACHE STRING "Documentation format")
else()
  find_program(MANTOOL NAMES nroff)
  if (MANTOOL)
    set(DOCUMENTATION_FORMAT "man" CACHE STRING "Documentation format")
  else()
    set(DOCUMENTATION_FORMAT "html" CACHE STRING "Documentation format")
  endif()
endif()

include(Dist)
Dist(${CMAKE_PROJECT_NAME}-${CMAKE_PROJECT_VERSION})

#ADD_CUSTOM_TARGET(uninstall
#  COMMAND cat ${PROJECT_BINARY_DIR}/install_manifest.txt | xargs rm
#  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
#  )

if(BUILD_SHARED_LIBS)
  set(HAVE_SHARED TRUE)
else()
  set(ZIP_STATIC TRUE)
endif()

# Checks

# Request ISO C secure library functions (memcpy_s &c)
list(APPEND CMAKE_REQUIRED_DEFINITIONS -D__STDC_WANT_LIB_EXT1__=1)

check_function_exists(_close HAVE__CLOSE)
check_function_exists(_dup HAVE__DUP)
check_function_exists(_fdopen HAVE__FDOPEN)
check_function_exists(_fileno HAVE__FILENO)
check_function_exists(_setmode HAVE__SETMODE)
check_symbol_exists(_snprintf stdio.h HAVE__SNPRINTF)
check_symbol_exists(_snprintf_s stdio.h HAVE__SNPRINTF_S)
check_symbol_exists(_snwprintf_s stdio.h HAVE__SNWPRINTF_S)
check_function_exists(_strdup HAVE__STRDUP)
check_symbol_exists(_stricmp string.h HAVE__STRICMP)
check_function_exists(_strtoi64 HAVE__STRTOI64)
check_function_exists(_strtoui64 HAVE__STRTOUI64)
check_function_exists(_unlink HAVE__UNLINK)
check_function_exists(arc4random HAVE_ARC4RANDOM)
check_function_exists(clonefile HAVE_CLONEFILE)
check_function_exists(explicit_bzero HAVE_EXPLICIT_BZERO)
check_function_exists(explicit_memset HAVE_EXPLICIT_MEMSET)
check_function_exists(fchmod HAVE_FCHMOD)
check_function_exists(fileno HAVE_FILENO)
check_function_exists(fseeko HAVE_FSEEKO)
check_function_exists(ftello HAVE_FTELLO)
check_function_exists(getprogname HAVE_GETPROGNAME)
check_symbol_exists(localtime_r time.h HAVE_LOCALTIME_R)
check_symbol_exists(localtime_s time.h HAVE_LOCALTIME_S)
check_function_exists(memcpy_s HAVE_MEMCPY_S)
check_function_exists(setmode HAVE_SETMODE)
check_symbol_exists(snprintf stdio.h HAVE_SNPRINTF)
check_symbol_exists(snprintf_s stdio.h HAVE_SNPRINTF_S)
check_symbol_exists(strcasecmp strings.h HAVE_STRCASECMP)
check_function_exists(strdup HAVE_STRDUP)
check_function_exists(strerror_s HAVE_STRERROR_S)
check_function_exists(strerrorlen_s HAVE_STRERRORLEN_S)
check_function_exists(stricmp HAVE_STRICMP)
check_function_exists(strncpy_s HAVE_STRNCPY_S)
check_function_exists(strtoll HAVE_STRTOLL)
check_function_exists(strtoull HAVE_STRTOULL)

check_include_files("sys/types.h;sys/stat.h;fts.h" HAVE_FTS_H)
# fts functions may be in external library
if(HAVE_FTS_H)
  check_function_exists(fts_open HAVE_FTS_OPEN)
  if(NOT HAVE_FTS_OPEN)
    check_library_exists(fts fts_open "" HAVE_LIB_FTS)
  else(NOT HAVE_FTS_OPEN)
    set(HAVE_LIB_FTS "" CACHE INTERNAL "")
  endif(NOT HAVE_FTS_OPEN)
else(HAVE_FTS_H)
  set(HAVE_LIB_FTS "" CACHE INTERNAL "")
endif(HAVE_FTS_H)

if(HAVE_LIB_FTS)
  set(FTS_LIB fts CACHE INTERNAL "")
else()
  set(FTS_LIB "" CACHE INTERNAL "")
endif()

check_include_files(stdbool.h HAVE_STDBOOL_H)
check_include_files(strings.h HAVE_STRINGS_H)
check_include_files(unistd.h HAVE_UNISTD_H)

check_include_files(inttypes.h HAVE_INTTYPES_H_LIBZIP)
check_include_files(stdint.h HAVE_STDINT_H_LIBZIP)
check_include_files(sys/types.h HAVE_SYS_TYPES_H_LIBZIP)

# TODO: fix test
# this test does not find __progname even when it exists
#check_symbol_exists(__progname stdlib.h HAVE___PROGNAME)

check_type_size(__int8 __INT8_LIBZIP)
check_type_size(int8_t INT8_T_LIBZIP)
check_type_size(uint8_t UINT8_T_LIBZIP)
check_type_size(__int16 __INT16_LIBZIP)
check_type_size(int16_t INT16_T_LIBZIP)
check_type_size(uint16_t UINT16_T_LIBZIP)
check_type_size(__int32 __INT32_LIBZIP)
check_type_size(int32_t INT32_T_LIBZIP)
check_type_size(uint32_t UINT32_T_LIBZIP)
check_type_size(__int64 __INT64_LIBZIP)
check_type_size(int64_t INT64_T_LIBZIP)
check_type_size(uint64_t UINT64_T_LIBZIP)
check_type_size("short" SHORT_LIBZIP)
check_type_size("int" INT_LIBZIP)
check_type_size("long" LONG_LIBZIP)
check_type_size("long long" LONG_LONG_LIBZIP)
check_type_size("off_t" SIZEOF_OFF_T)
check_type_size("size_t" SIZEOF_SIZE_T)

check_c_source_compiles("#include <sys/ioctl.h>
#include <linux/fs.h>
int main(int argc, char *argv[]) { unsigned long x = FICLONERANGE; }" HAVE_FICLONERANGE)

check_c_source_compiles("
int foo(char * _Nullable bar);
int main(int argc, char *argv[]) { }" HAVE_NULLABLE)

test_big_endian(WORDS_BIGENDIAN)

find_package(ZLIB 1.1.2 REQUIRED)
# so developers on systems where zlib is named differently (Windows, sometimes)
# can override the name used in the pkg-config file
if (NOT ZLIB_LINK_LIBRARY_NAME)
  set(ZLIB_LINK_LIBRARY_NAME "z")

  # Get the correct name in common cases
  list(LENGTH ZLIB_LIBRARIES N_ZLIB_LIBRARIES)
  if(N_ZLIB_LIBRARIES EQUAL 1)
    set(ZLIB_FILENAME ${ZLIB_LIBRARIES})
  elseif(N_ZLIB_LIBRARIES EQUAL 4)
    # ZLIB_LIBRARIES might have the target_link_library() format like
    # "optimized;path/to/zlib.lib;debug;path/to/zlibd.lib". Use the 'optimized'
    # case unless we know we are in a Debug build.
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
      list(FIND ZLIB_LIBRARIES "debug" ZLIB_LIBRARIES_INDEX_OF_CONFIG)
    else()
      list(FIND ZLIB_LIBRARIES "optimized" ZLIB_LIBRARIES_INDEX_OF_CONFIG)
    endif()
    if(ZLIB_LIBRARIES_INDEX_OF_CONFIG GREATER_EQUAL 0)
      math(EXPR ZLIB_FILENAME_INDEX "${ZLIB_LIBRARIES_INDEX_OF_CONFIG}+1")
      list(GET ZLIB_LIBRARIES ${ZLIB_FILENAME_INDEX} ZLIB_FILENAME)
    endif()
  endif()
  if(ZLIB_FILENAME)
    get_filename_component(ZLIB_FILENAME ${ZLIB_FILENAME} NAME_WE)
    string(REGEX REPLACE "^lib" "" ZLIB_LINK_LIBRARY_NAME ${ZLIB_FILENAME})
  endif()
endif(NOT ZLIB_LINK_LIBRARY_NAME)

if(ENABLE_BZIP2)
  find_package(BZip2)
  if(BZIP2_FOUND)
    set(HAVE_LIBBZ2 1)
  else()
    message(WARNING "-- bzip2 library not found; bzip2 support disabled")
  endif(BZIP2_FOUND)
endif(ENABLE_BZIP2)

if(ENABLE_LZMA)
  find_package(LibLZMA 5.2)
  if(LIBLZMA_FOUND)
    set(HAVE_LIBLZMA 1)
  else()
    message(WARNING "-- lzma library not found; lzma/xz support disabled")
  endif(LIBLZMA_FOUND)
endif(ENABLE_LZMA)

if(ENABLE_ZSTD)
  find_package(Zstd 1.3.6)
  if(Zstd_FOUND)
    set(HAVE_LIBZSTD 1)
  else()
    message(WARNING "-- zstd library not found; zstandard support disabled")
  endif(Zstd_FOUND)
endif(ENABLE_ZSTD)

if (COMMONCRYPTO_FOUND)
  set(HAVE_CRYPTO 1)
  set(HAVE_COMMONCRYPTO 1)
elseif (WINDOWS_CRYPTO_FOUND)
  set(HAVE_CRYPTO 1)
  set(HAVE_WINDOWS_CRYPTO 1)
elseif (GNUTLS_FOUND AND NETTLE_FOUND)
  set(HAVE_CRYPTO 1)
  set(HAVE_GNUTLS 1)
elseif (OPENSSL_FOUND)
  set(HAVE_CRYPTO 1)
  set(HAVE_OPENSSL 1)
elseif (MBEDTLS_FOUND)
  set(HAVE_CRYPTO 1)
  set(HAVE_MBEDTLS 1)
endif()

if ((ENABLE_COMMONCRYPTO OR ENABLE_GNUTLS OR ENABLE_MBEDTLS OR ENABLE_OPENSSL OR ENABLE_WINDOWS_CRYPTO) AND NOT HAVE_CRYPTO)
  message(WARNING "-- neither Common Crypto, GnuTLS, mbed TLS, OpenSSL, nor Windows Cryptography found; AES support disabled")
endif()

if(MSVC)
  add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
  add_compile_definitions(_CRT_NONSTDC_NO_DEPRECATE)
endif(MSVC)

if(WIN32)
  if(CMAKE_SYSTEM_NAME MATCHES WindowsPhone OR CMAKE_SYSTEM_NAME MATCHES WindowsStore)
    add_compile_definitions(MS_UWP)
  endif(CMAKE_SYSTEM_NAME MATCHES WindowsPhone OR CMAKE_SYSTEM_NAME MATCHES WindowsStore)
endif(WIN32)

# rpath handling: use rpath in installed binaries
if(NOT CMAKE_SYSTEM_NAME MATCHES Linux)
  set(CMAKE_INSTALL_RPATH ${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR})
  set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
endif()

# for code completion frameworks
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Testing
ENABLE_TESTING()

# Targets
ADD_SUBDIRECTORY(lib)

if(BUILD_DOC)
  ADD_SUBDIRECTORY(man)
endif()

if(BUILD_TOOLS)
  ADD_SUBDIRECTORY(src)
else(BUILD_TOOLS)
  if(BUILD_REGRESS)
    message(WARNING "-- tools build has been disabled, but they are needed for regression tests; regression testing disabled")
    set(BUILD_REGRESS OFF)
  endif(BUILD_REGRESS)
endif()

include(FindPerl)

if(BUILD_REGRESS AND NOT PERL_FOUND)
  message(WARNING "-- perl not found, regression testing disabled")
  set(BUILD_REGRESS OFF)
endif()

if(BUILD_REGRESS)
  add_subdirectory(regress)
endif()

if(BUILD_EXAMPLES)
  add_subdirectory(examples)
endif()


# pkgconfig file
file(RELATIVE_PATH pc_relative_bindir ${CMAKE_INSTALL_PREFIX} ${CMAKE_INSTALL_FULL_BINDIR})
set(bindir "\${prefix}/${pc_relative_bindir}")
file(RELATIVE_PATH pc_relative_libdir ${CMAKE_INSTALL_PREFIX} ${CMAKE_INSTALL_FULL_LIBDIR})
set(libdir "\${prefix}/${pc_relative_libdir}")
file(RELATIVE_PATH pc_relative_includedir ${CMAKE_INSTALL_PREFIX} ${CMAKE_INSTALL_FULL_INCLUDEDIR})
set(includedir "\${prefix}/${pc_relative_includedir}")
if(CMAKE_SYSTEM_NAME MATCHES BSD)
  set(PKG_CONFIG_RPATH "-Wl,-R\${libdir}")
endif(CMAKE_SYSTEM_NAME MATCHES BSD)
get_target_property(LIBS_PRIVATE zip LINK_LIBRARIES)
foreach(LIB ${LIBS_PRIVATE})
  if(LIB MATCHES "^/")
    get_filename_component(LIB ${LIB} NAME_WE)
    string(REGEX REPLACE "^lib" "" LIB ${LIB})
  endif()
  set(LIBS "${LIBS} -l${LIB}")
endforeach()
STRING(CONCAT zlib_link_name "-l" ${ZLIB_LINK_LIBRARY_NAME})
string(REGEX REPLACE "-lBZip2::BZip2" "-lbz2" LIBS ${LIBS})
string(REGEX REPLACE "-lLibLZMA::LibLZMA" "-llzma" LIBS ${LIBS})
string(REGEX REPLACE "-lZstd::Zstd" "-lzstd" LIBS ${LIBS})
string(REGEX REPLACE "-lOpenSSL::Crypto" "-lssl -lcrypto" LIBS ${LIBS})
string(REGEX REPLACE "-lZLIB::ZLIB" ${zlib_link_name} LIBS ${LIBS})
string(REGEX REPLACE "-lGnuTLS::GnuTLS" "-lgnutls" LIBS ${LIBS})
string(REGEX REPLACE "-lNettle::Nettle" "-lnettle" LIBS ${LIBS})
configure_file(libzip.pc.in libzip.pc @ONLY)
if(LIBZIP_DO_INSTALL)
  install(FILES ${PROJECT_BINARY_DIR}/libzip.pc DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig)
endif()

# fixed size integral types

if(HAVE_INTTYPES_H_LIBZIP)
  set(LIBZIP_TYPES_INCLUDE "#if !defined(__STDC_FORMAT_MACROS)
#define __STDC_FORMAT_MACROS 1
#endif
#include <inttypes.h>")
elseif(HAVE_STDINT_H_LIBZIP)
  set(LIBZIP_TYPES_INCLUDE "#include <stdint.h>")
elseif(HAVE_SYS_TYPES_H_LIBZIP)
  set(LIBZIP_TYPES_INCLUDE "#include <sys/types.h>")
endif()

if(HAVE_INT8_T_LIBZIP)
  set(ZIP_INT8_T int8_t)
elseif(HAVE___INT8_LIBZIP)
  set(ZIP_INT8_T __int8)
else()
  set(ZIP_INT8_T "signed char")
endif()

if(HAVE_UINT8_T_LIBZIP)
  set(ZIP_UINT8_T uint8_t)
elseif(HAVE___INT8_LIBZIP)
  set(ZIP_UINT8_T "unsigned __int8")
else()
  set(ZIP_UINT8_T "unsigned char")
endif()

if(HAVE_INT16_T_LIBZIP)
  set(ZIP_INT16_T int16_t)
elseif(HAVE___INT16_LIBZIP)
  set(INT16_T_LIBZIP __int16)
elseif(SHORT_LIBZIP EQUAL 2)
  set(INT16_T_LIBZIP short)
endif()

if(HAVE_UINT16_T_LIBZIP)
  set(ZIP_UINT16_T uint16_t)
elseif(HAVE___INT16_LIBZIP)
  set(UINT16_T_LIBZIP "unsigned __int16")
elseif(SHORT_LIBZIP EQUAL 2)
  set(UINT16_T_LIBZIP "unsigned short")
endif()

if(HAVE_INT32_T_LIBZIP)
  set(ZIP_INT32_T int32_t)
elseif(HAVE___INT32_LIBZIP)
  set(ZIP_INT32_T __int32)
elseif(INT_LIBZIP EQUAL 4)
  set(ZIP_INT32_T int)
elseif(LONG_LIBZIP EQUAL 4)
  set(ZIP_INT32_T long)
endif()

if(HAVE_UINT32_T_LIBZIP)
  set(ZIP_UINT32_T uint32_t)
elseif(HAVE___INT32_LIBZIP)
  set(ZIP_UINT32_T "unsigned __int32")
elseif(INT_LIBZIP EQUAL 4)
  set(ZIP_UINT32_T "unsigned int")
elseif(LONG_LIBZIP EQUAL 4)
  set(ZIP_UINT32_T "unsigned long")
endif()

if(HAVE_INT64_T_LIBZIP)
  set(ZIP_INT64_T int64_t)
elseif(HAVE___INT64_LIBZIP)
  set(ZIP_INT64_T __int64)
elseif(LONG_LIBZIP EQUAL 8)
  set(ZIP_INT64_T long)
elseif(LONG_LONG_LIBZIP EQUAL 8)
  set(ZIP_INT64_T "long long")
endif()

if(HAVE_UINT64_T_LIBZIP)
  set(ZIP_UINT64_T uint64_t)
elseif(HAVE___INT64_LIBZIP)
  set(ZIP_UINT64_T "unsigned __int64")
elseif(LONG_LIBZIP EQUAL 8)
  set(ZIP_UINT64_T "unsigned long")
elseif(LONG_LONG_LIBZIP EQUAL 8)
  set(ZIP_UINT64_T "unsigned long long")
endif()

if(HAVE_NULLABLE)
  set(ZIP_NULLABLE_DEFINES)
else()
  set(ZIP_NULLABLE_DEFINES "#define _Nullable
#define _Nonnull")
endif()

# write out config file
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/cmake-config.h.in ${PROJECT_BINARY_DIR}/config.h)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/cmake-zipconf.h.in ${PROJECT_BINARY_DIR}/zipconf.h)

# for tests

set(srcdir ${CMAKE_CURRENT_SOURCE_DIR}/regress)
set(abs_srcdir ${CMAKE_CURRENT_SOURCE_DIR}/regress)
set(top_builddir ${PROJECT_BINARY_DIR}) # used to find config.h

configure_file(regress/nihtest.conf.in ${PROJECT_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/nihtest.conf @ONLY)
file(COPY ${PROJECT_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/nihtest.conf
  DESTINATION ${PROJECT_BINARY_DIR}/regress
  FILE_PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)

configure_file(regress/runtest.in ${PROJECT_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/runtest @ONLY)
file(COPY ${PROJECT_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/runtest
  DESTINATION ${PROJECT_BINARY_DIR}/regress
  FILE_PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
  )

# create package config file
include(CMakePackageConfigHelpers)
write_basic_package_version_file("${PROJECT_BINARY_DIR}/${PROJECT_NAME}-config-version.cmake"
  COMPATIBILITY AnyNewerVersion)

configure_package_config_file("${PROJECT_NAME}-config.cmake.in" "${PROJECT_BINARY_DIR}/${PROJECT_NAME}-config.cmake"
  INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/libzip)

if(LIBZIP_DO_INSTALL)
  # Add targets to the build-tree export set
  export(TARGETS zip
    FILE "${PROJECT_BINARY_DIR}/${PROJECT_NAME}-targets.cmake")

  # installation
  install(FILES ${PROJECT_BINARY_DIR}/zipconf.h DESTINATION include)
  install(FILES ${PROJECT_BINARY_DIR}/${PROJECT_NAME}-config.cmake ${PROJECT_BINARY_DIR}/${PROJECT_NAME}-config-version.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}
    )
  install(EXPORT ${PROJECT_NAME}-targets NAMESPACE libzip:: FILE ${PROJECT_NAME}-targets.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}
    )
endif()
