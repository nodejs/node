cmake_minimum_required(VERSION 3.15...3.31)

project(
    zlib
    LANGUAGES C
    VERSION 1.4
    HOMEPAGE_URL "https://zlib.net/"
    DESCRIPTION "a general-purpose lossless data-compression library")

# ============================================================================
# CPack
# ============================================================================
set(CPACK_PACKAGE_VENDOR "zlib-Project")
set(CPACK_PACKAGE_DESCRIPTION_FILE ${zlib_SOURCE_DIR}/README)
set(CPACK_RESOURCE_FILE_LICENSE ${zlib_SOURCE_DIR}/LICENSE)
set(CPACK_RESOURCE_FILE_README ${zlib_SOURCE_DIR}/README)

# ============================================================================
# configuration
# ============================================================================

option(ZLIB_BUILD_TESTING "Enable Zlib Examples as tests" ON)
option(ZLIB_BUILD_SHARED "Enable building zlib shared library" ON)
option(ZLIB_BUILD_STATIC "Enable building zlib static library" ON)
option(ZLIB_INSTALL "Enable installation of zlib" ON)
set(ZLIB_PREFIX "" CACHE STRING
    "prefix for all types and library functions, see zconf.h.in")
mark_as_advanced(ZLIB_PREFIX)

get_property(IS_MULTI GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)

if(NOT DEFINED CMAKE_BUILD_TYPE AND NOT IS_MULTI)
    message(STATUS "No CMAKE_BUILD_TYPE set -- using Release")
    set(CMAKE_BUILD_TYPE Release)
endif(NOT DEFINED CMAKE_BUILD_TYPE AND NOT IS_MULTI)

include(CheckCSourceCompiles)
include(CheckSymbolExists)
include(CheckIncludeFile)
include(CMakePackageConfigHelpers)
include(CheckTypeSize)
include(CPack)
include(GNUInstallDirs)

set(CPACK_INCLUDED TRUE)

set(Z_PREFIX "${ZLIB_PREFIX}")

#
# Create zconf.h.cmakein
#
if(NOT ZLIB_CONF_WRITTEN)
    set(CONF_OUT_FILE ${zlib_BINARY_DIR}/zconf.h.cmakein)
    file(READ ${zlib_SOURCE_DIR}/zconf.h ZCONF_CONTENT LIMIT 245)
    file(WRITE ${CONF_OUT_FILE} ${ZCONF_CONTENT})
    file(APPEND ${CONF_OUT_FILE} "#cmakedefine Z_PREFIX @Z_PREFIX@\n")
    file(APPEND ${CONF_OUT_FILE} "#cmakedefine HAVE_STDARG_H 1\n")
    file(APPEND ${CONF_OUT_FILE} "#cmakedefine HAVE_UNISTD_H 1\n")
    file(READ ${zlib_SOURCE_DIR}/zconf.h ZCONF_CONTENT OFFSET 244)
    set(FIRST_ITEM TRUE)

    foreach(item IN LISTS ZCONF_CONTENT)
        if(FIRST_ITEM)
            string(APPEND OUT_CONTENT ${item})
            set(FIRST_ITEM FALSE)
        else(FIRST_ITEM)
            string(APPEND OUT_CONTENT "\;" ${item})
        endif(FIRST_ITEM)
    endforeach(item IN LISTS ${ZCONF_CONTENT})

    file(APPEND ${CONF_OUT_FILE} ${OUT_CONTENT})
    set(ZLIB_CONF_WRITTEN
        TRUE
        CACHE BOOL "zconf.h.cmakein was created")
    mark_as_advanced(ZLIB_CONF_WRITTEN)
endif(NOT ZLIB_CONF_WRITTEN)

option(ENABLE_SIMD_OPTIMIZATIONS "Enable all SIMD optimizations" OFF)
option(ENABLE_SIMD_AVX512 "Enable SIMD AXV512 optimizations" OFF)
option(USE_ZLIB_RABIN_KARP_HASH "Enable bitstream compatibility with canonical zlib" OFF)
option(ENABLE_INTEL_QAT_COMPRESSION "Enable Intel Quick Assist Technology use for compression" OFF)
option(BUILD_UNITTESTS "Enable standalone unit tests build" OFF)
option(BUILD_MINIZIP_BIN "Enable building minzip_bin tool" OFF)
option(BUILD_ZPIPE "Enable building zpipe tool" OFF)
option(BUILD_MINIGZIP "Enable building minigzip tool" OFF)

if (USE_ZLIB_RABIN_KARP_HASH)
   add_definitions(-DUSE_ZLIB_RABIN_KARP_ROLLING_HASH)
endif()

# TODO(cavalcantii): add support for other OSes (e.g. Android, Fuchsia, etc)
# and architectures (e.g. RISCV).
if (ENABLE_SIMD_OPTIMIZATIONS)
  # Apparently some environments (e.g. CentOS) require to explicitly link
  # with pthread and that is required by the CPU features detection code.
  find_package (Threads REQUIRED)
  SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -pthread")
  SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread")

  if (CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64")
    add_definitions(-DDEFLATE_COMPARE256_64LE)
    add_definitions(-DINFLATE_CHUNK_SIMD_SSE2)
    add_definitions(-DADLER32_SIMD_SSSE3)
    add_definitions(-DINFLATE_CHUNK_READ_64LE)
    add_definitions(-DDEFLATE_CHUNK_WRITE_64LE)
    add_definitions(-DCRC32_SIMD_SSE42_PCLMUL)
    if (ENABLE_SIMD_AVX512)
      add_definitions(-DCRC32_SIMD_AVX512_PCLMUL)
    endif()
    add_definitions(-DDEFLATE_SLIDE_HASH_SSE2)
    # Required by CPU features detection code.
    add_definitions(-DX86_NOT_WINDOWS)
  endif()

  if ((CMAKE_SYSTEM_PROCESSOR STREQUAL "arm64") OR
      (CMAKE_SYSTEM_PROCESSOR STREQUAL "aarch64"))
    add_definitions(-DINFLATE_CHUNK_SIMD_NEON)
    add_definitions(-DADLER32_SIMD_NEON)
    add_definitions(-DINFLATE_CHUNK_READ_64LE)
    add_definitions(-DDEFLATE_CHUNK_WRITE_64LE)
    add_definitions(-DCRC32_ARMV8_CRC32)
    add_definitions(-DDEFLATE_SLIDE_HASH_NEON)
    add_definitions(-DDEFLATE_COMPARE256_64LE)
    # Required by CPU features detection code.
    if (APPLE)
      add_definitions(-DARMV8_OS_MACOS)
    endif()

    if (UNIX AND NOT APPLE)
      add_definitions(-DARMV8_OS_LINUX)
    endif()

    SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -march=armv8-a+crc+crypto")
  endif()

  if (CMAKE_SYSTEM_PROCESSOR STREQUAL "riscv64")
    add_definitions(-DRISCV_RVV)
    add_definitions(-DDEFLATE_SLIDE_HASH_RVV)
    add_definitions(-DADLER32_SIMD_RVV)

    # chunk_copy is required for READ64 and unconditional decode of literals.
    add_definitions(-DINFLATE_CHUNK_GENERIC)
    add_definitions(-DINFLATE_CHUNK_READ_64LE)
    add_definitions(-DDEFLATE_CHUNK_WRITE_64LE)

    # TODO(cavalcantii): only missing optimization is SLIDE_HASH, got port it
    # to RISCV.

    # Tested with clang-17, unaligned loads are required by read64 & chunk_copy.
    # TODO(cavalcantii): replace internal clang flags for -munaligned-access
    # when we have a newer compiler available.
    SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} --target=riscv64-unknown-linux-gnu -march=rv64gcv -Xclang -target-feature -Xclang +unaligned-scalar-mem")
  endif()
endif()

#
# Create zlib.def.cmakein
#
if(NOT ZLIB_DEF_WRITTEN)
    file(STRINGS "${zlib_SOURCE_DIR}/win32/zlib.def" ZLIB_DEF_LINES)

    set(ZLIB_DEF_OUT "")

    foreach(line IN LISTS ZLIB_DEF_LINES)
        if(line MATCHES "^;")
            string(APPEND ZLIB_DEF_OUT "${line}\n")
        elseif(line MATCHES "^(LIBRARY|EXPORTS|DESCRIPTION)")
            string(APPEND ZLIB_DEF_OUT "${line}\n")
        elseif(line MATCHES "^ *$")
            string(APPEND ZLIB_DEF_OUT "\n")
        elseif(line MATCHES "^ *[A-Za-z_][A-Za-z0-9_]*")
            string(REGEX REPLACE "^ +" "" sym "${line}")
            string(APPEND ZLIB_DEF_OUT
                    "    @ZLIB_PREFIX@${sym}\n")
        else()
            string(APPEND ZLIB_DEF_OUT "${line}\n")
        endif()
    endforeach()

    file(WRITE
      "${CMAKE_CURRENT_BINARY_DIR}/zlib.def.cmakein"
      "${ZLIB_DEF_OUT}"
    )

    set(ZLIB_DEF_WRITTEN
        TRUE
        CACHE BOOL "zlib.def.cmakein was created")
    mark_as_advanced(ZLIB_DEF_WRITTEN)
endif(NOT ZLIB_DEF_WRITTEN)

#
#Create zlib.map
#
if(NOT ZLIB_MAP_WRITTEN)
    file(STRINGS "${zlib_SOURCE_DIR}/zlib.map" ZLIB_MAP_CONTENT)
    set(ZLIB_MAP_OUT "")

    foreach(line IN LISTS ZLIB_MAP_CONTENT)
        if(line MATCHES "^ *[A-Za-z_][A-Za-z0-9_]* *; *$")
            if(line MATCHES "^ *\\* *; *$")
                string(APPEND ZLIB_MAP_OUT "${line}\n")
            else()
                string(REGEX REPLACE "^ +" "" sym "${line}")
                string(APPEND ZLIB_MAP_OUT
                       "    @ZLIB_PREFIX@${sym}\n")
            endif()
        else()
            string(APPEND ZLIB_MAP_OUT "${line}\n")
        endif()
    endforeach()

    file(WRITE
      "${zlib_BINARY_DIR}/zlib.map.cmakein"
      "${ZLIB_MAP_OUT}"
    )

    set(ZLIB_MAP_WRITTEN
        TRUE
        CACHE BOOL "zlib.map.cmakein was created")
    mark_as_advanced(ZLIB_MAP_WRITTEN)
endif(NOT ZLIB_MAP_WRITTEN)

#
# Check to see if we have large file support
#
set(CMAKE_REQUIRED_DEFINITIONS -D_LARGEFILE64_SOURCE=1)
check_type_size(off64_t OFF64_T)
unset(CMAKE_REQUIRED_DEFINITIONS) # clear variable

#
# Check for fseeko
#
check_symbol_exists(fseeko stdio.h HAVE_FSEEKO)

#
# Check for stdarg.h
#
check_include_file(stdarg.h HAVE_STDARG_H)

#
# Check for unistd.h
#
check_include_file(unistd.h HAVE_UNISTD_H)

#
# Check visibility attribute is supported
#
if(MSVC)
    set(CMAKE_REQUIRED_FLAGS "-WX")
else(MSVC)
    set(CMAKE_REQUIRED_FLAGS "-Werror")
endif(MSVC)

check_c_source_compiles(
    "
        #include <stdlib.h>
        static void f(void) __attribute__ ((visibility(\"hidden\")));
        int main(void) {return 0;}
    "
    HAVE___ATTR__VIS_HIDDEN)

unset(CMAKE_COMPILE_FLAGS)
set(ZLIB_PC ${zlib_BINARY_DIR}/zlib.pc)
if(IS_ABSOLUTE "${CMAKE_INSTALL_INCLUDEDIR}")
    set(pc_includedir "${CMAKE_INSTALL_INCLUDEDIR}")
else()
    set(pc_includedir "\${prefix}/${CMAKE_INSTALL_INCLUDEDIR}")
endif()
if(IS_ABSOLUTE "${CMAKE_INSTALL_LIBDIR}")
    set(pc_libdir "${CMAKE_INSTALL_LIBDIR}")
else()
    set(pc_libdir "\${exec_prefix}/${CMAKE_INSTALL_LIBDIR}")
endif()

configure_file(${zlib_SOURCE_DIR}/zlib.pc.cmakein ${ZLIB_PC} @ONLY)
configure_file(${zlib_BINARY_DIR}/zconf.h.cmakein ${zlib_BINARY_DIR}/zconf.h)
configure_file(${zlib_BINARY_DIR}/zlib.def.cmakein ${zlib_BINARY_DIR}/zlib.def)
configure_file(${zlib_BINARY_DIR}/zlib.map.cmakein ${zlib_BINARY_DIR}/zlib.map)

# ============================================================================
# zlib
# ============================================================================

set(ZLIB_PUBLIC_HDRS ${zlib_BINARY_DIR}/zconf.h zlib.h)

set(ZLIB_PRIVATE_HDRS
    crc32.h
    deflate.h
    gzguts.h
    inffast.h
    inffixed.h
    inflate.h
    inftrees.h
    trees.h
    zutil.h)

set(ZLIB_SRCS
    adler32.c
    compress.c
    crc32.c
    deflate.c
    gzclose.c
    gzlib.c
    gzread.c
    gzwrite.c
    inflate.c
    infback.c
    inftrees.c
    inffast.c
    trees.c
    uncompr.c
    zutil.c)

if(WIN32)
    set(zlib_static_suffix "s")
    set(CMAKE_DEBUG_POSTFIX "d")
endif(WIN32)


#============================================================================
# Update list of source files if optimizations were enabled
#============================================================================
if (ENABLE_SIMD_OPTIMIZATIONS)
  if (CMAKE_SYSTEM_PROCESSOR STREQUAL "riscv64")
    message("RISCVV: Add optimizations.")
    list(REMOVE_ITEM ZLIB_SRCS inflate.c)
    list(APPEND ZLIB_PRIVATE_HDRS ${CMAKE_CURRENT_SOURCE_DIR}/adler32_simd.h)
    list(APPEND ZLIB_PRIVATE_HDRS ${CMAKE_CURRENT_SOURCE_DIR}/contrib/optimizations/chunkcopy.h)
    list(APPEND ZLIB_PRIVATE_HDRS ${CMAKE_CURRENT_SOURCE_DIR}/cpu_features.h)

    list(APPEND ZLIB_SRCS ${CMAKE_CURRENT_SOURCE_DIR}/adler32_simd.c)
    list(APPEND ZLIB_SRCS ${CMAKE_CURRENT_SOURCE_DIR}/contrib/optimizations/inffast_chunk.c)
    list(APPEND ZLIB_SRCS ${CMAKE_CURRENT_SOURCE_DIR}/contrib/optimizations/inflate.c)
    list(APPEND ZLIB_SRCS ${CMAKE_CURRENT_SOURCE_DIR}/cpu_features.c)
  else()
    list(REMOVE_ITEM ZLIB_SRCS inflate.c)

    list(APPEND ZLIB_PRIVATE_HDRS ${CMAKE_CURRENT_SOURCE_DIR}/adler32_simd.h)
    list(APPEND ZLIB_PRIVATE_HDRS ${CMAKE_CURRENT_SOURCE_DIR}/contrib/optimizations/chunkcopy.h)
    list(APPEND ZLIB_PRIVATE_HDRS ${CMAKE_CURRENT_SOURCE_DIR}/contrib/optimizations/inffast_chunk.h)
    list(APPEND ZLIB_PRIVATE_HDRS ${CMAKE_CURRENT_SOURCE_DIR}/cpu_features.h)
    list(APPEND ZLIB_PRIVATE_HDRS ${CMAKE_CURRENT_SOURCE_DIR}/crc32_simd.h)

    list(APPEND ZLIB_SRCS ${CMAKE_CURRENT_SOURCE_DIR}/adler32_simd.c)
    list(APPEND ZLIB_SRCS ${CMAKE_CURRENT_SOURCE_DIR}/contrib/optimizations/inffast_chunk.c)
    list(APPEND ZLIB_SRCS ${CMAKE_CURRENT_SOURCE_DIR}/contrib/optimizations/inflate.c)
    list(APPEND ZLIB_SRCS ${CMAKE_CURRENT_SOURCE_DIR}/cpu_features.c)
    list(APPEND ZLIB_SRCS ${CMAKE_CURRENT_SOURCE_DIR}/crc32_simd.c)
    list(APPEND ZLIB_SRCS ${CMAKE_CURRENT_SOURCE_DIR}/crc_folding.c)
  endif()
endif()

if (ENABLE_INTEL_QAT_COMPRESSION)
    list(APPEND ZLIB_SRCS ${CMAKE_CURRENT_SOURCE_DIR}/contrib/qat/deflate_qat.cpp)
    list(APPEND ZLIB_SRCS ${CMAKE_CURRENT_SOURCE_DIR}/contrib/qat/qatzpp/io_buffers.cpp)
    list(APPEND ZLIB_SRCS ${CMAKE_CURRENT_SOURCE_DIR}/contrib/qat/qatzpp/memory.cpp)
    list(APPEND ZLIB_SRCS ${CMAKE_CURRENT_SOURCE_DIR}/contrib/qat/qatzpp/qat_buffer_list.cpp)
    list(APPEND ZLIB_SRCS ${CMAKE_CURRENT_SOURCE_DIR}/contrib/qat/qatzpp/qat.cpp)
    list(APPEND ZLIB_SRCS ${CMAKE_CURRENT_SOURCE_DIR}/contrib/qat/qatzpp/qat_instance.cpp)
    list(APPEND ZLIB_SRCS ${CMAKE_CURRENT_SOURCE_DIR}/contrib/qat/qatzpp/session.cpp)
    list(APPEND ZLIB_SRCS ${CMAKE_CURRENT_SOURCE_DIR}/contrib/qat/qatzpp/qat_task.cpp)

    # TODO(gustavoa): Find a way to include the qatzpp headers without having the
    # presubmit check throw errors.
    include_directories(${CMAKE_CURRENT_SOURCE_DIR}/contrib/qat/qatzpp)
    add_compile_definitions(QAT_COMPRESSION_ENABLED)
endif()

if (ENABLE_INTEL_QAT_COMPRESSION)
    target_include_directories(zlib PUBLIC ${QATZPP_INCLUDE_DIRS})
    target_link_libraries(zlib ${QATZPP_LIBRARY})
    target_link_libraries(zlib qat)
    target_include_directories(zlibstatic PUBLIC ${QATZPP_INCLUDE_DIRS})
    target_link_libraries(zlibstatic ${QATZPP_LIBRARY})
    target_link_libraries(zlibstatic qat)
endif()

if(ZLIB_BUILD_SHARED)
    add_library(
        zlib SHARED ${ZLIB_SRCS} ${ZLIB_PUBLIC_HDRS}
                    ${ZLIB_PRIVATE_HDRS}
                    $<$<OR:$<BOOL:${WIN32}>,$<BOOL:${CYGWIN}>>:win32/zlib-cmake.rc>
                    $<$<BOOL:${MSVC}>:${zlib_BINARY_DIR}/zlib.def>)
    add_library(ZLIB::ZLIB ALIAS zlib)

    target_include_directories(
        zlib
        PUBLIC $<BUILD_INTERFACE:${zlib_BINARY_DIR}>
               $<BUILD_INTERFACE:${zlib_SOURCE_DIR}>
               $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
    target_compile_definitions(
        zlib
        PRIVATE ZLIB_BUILD
                $<$<NOT:$<BOOL:${HAVE_FSEEKO}>>:NO_FSEEKO>
                $<$<BOOL:${HAVE___ATTR__VIS_HIDDEN}>:HAVE_HIDDEN>
                $<$<BOOL:${MSVC}>:_CRT_SECURE_NO_DEPRECATE>
                $<$<BOOL:${MSVC}>:_CRT_NONSTDC_NO_DEPRECATE>
        PUBLIC $<$<BOOL:${HAVE_OFF64_T}>:_LARGEFILE64_SOURCE=1>
               $<$<BOOL:${ZLIB_PREFIX}>:Z_PREFIX=${ZLIB_PREFIX}>)

    set(INSTALL_VERSION ${zlib_VERSION})

    if(CYGWIN)
        set_target_properties(zlib PROPERTIES PREFIX "lib")
    else(CYGWIN)
        set_target_properties(zlib PROPERTIES SOVERSION ${zlib_VERSION_MAJOR}
                                              VERSION ${INSTALL_VERSION})
    endif(CYGWIN)

    set_target_properties(zlib
        PROPERTIES
            DEFINE_SYMBOL ZLIB_DLL
            EXPORT_NAME ZLIB
            PUBLIC_HEADER "${ZLIB_PUBLIC_HDRS}")

    if(MSVC)
        set_target_properties(zlib PROPERTIES OUTPUT_NAME libz)
    else(MSVC)
        set_target_properties(zlib PROPERTIES OUTPUT_NAME z)
    endif(MSVC)

    if(UNIX
        AND NOT APPLE
        AND NOT (CMAKE_SYSTEM_NAME STREQUAL "AIX")
        AND NOT (CMAKE_SYSTEM_NAME STREQUAL "SunOS"))
        # On unix-like platforms the library is almost always called libz
        set_target_properties(
            zlib
            PROPERTIES LINK_FLAGS
                       "-Wl,--version-script,\"${zlib_BINARY_DIR}/zlib.map\"")
    endif(
        UNIX
        AND NOT APPLE
        AND NOT (CMAKE_SYSTEM_NAME STREQUAL "AIX")
        AND NOT (CMAKE_SYSTEM_NAME STREQUAL "SunOS"))
endif(ZLIB_BUILD_SHARED)

if(ZLIB_BUILD_STATIC)
    add_library(zlibstatic STATIC ${ZLIB_SRCS} ${ZLIB_PUBLIC_HDRS}
                                  ${ZLIB_PRIVATE_HDRS})
    add_library(ZLIB::ZLIBSTATIC ALIAS zlibstatic)
    target_include_directories(
        zlibstatic
        PUBLIC $<BUILD_INTERFACE:${zlib_BINARY_DIR}>
               $<BUILD_INTERFACE:${zlib_SOURCE_DIR}>
               $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
    target_compile_definitions(
        zlibstatic
        PRIVATE ZLIB_BUILD
                $<$<NOT:$<BOOL:${HAVE_FSEEKO}>>:NO_FSEEKO>
                $<$<BOOL:${HAVE___ATTR__VIS_HIDDEN}>:HAVE_HIDDEN>
                $<$<BOOL:${MSVC}>:_CRT_SECURE_NO_DEPRECATE>
                $<$<BOOL:${MSVC}>:_CRT_NONSTDC_NO_DEPRECATE>
        PUBLIC $<$<BOOL:${HAVE_OFF64_T}>:_LARGEFILE64_SOURCE=1>
               $<$<BOOL:${ZLIB_PREFIX}>:Z_PREFIX=${ZLIB_PREFIX}>)

    if(CYGWIN)
        set_target_properties(zlibstatic PROPERTIES PREFIX "lib")
    endif(CYGWIN)

    if(MSVC)
        set_target_properties(zlibstatic
            PROPERTIES
                ARCHIVE_OUTPUT_NAME libz${zlib_static_suffix})
    else(MSVC)
        set_target_properties(zlibstatic
            PROPERTIES
                ARCHIVE_OUTPUT_NAME z${zlib_static_suffix})
    endif(MSVC)

    set_target_properties(zlibstatic
        PROPERTIES
            EXPORT_NAME ZLIBSTATIC
            PUBLIC_HEADER "${ZLIB_PUBLIC_HDRS}")
endif(ZLIB_BUILD_STATIC)

if(ZLIB_INSTALL)
    if(ZLIB_BUILD_SHARED)
        install(
            TARGETS zlib
            COMPONENT Runtime
            EXPORT zlibSharedExport
            RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
            ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
            LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
            PUBLIC_HEADER
                DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
                COMPONENT Development)
        install(
            EXPORT zlibSharedExport
            FILE ZLIB-shared.cmake
            NAMESPACE ZLIB::
            DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/zlib)

        if(MSVC)
            install(
                FILES $<TARGET_PDB_FILE:zlib>
                COMPONENT Runtime
                DESTINATION ${CMAKE_INSTALL_BINDIR}
                CONFIGURATIONS Debug OR RelWithDebInfo
                OPTIONAL)
        endif(MSVC)
    endif(ZLIB_BUILD_SHARED)

    if(ZLIB_BUILD_STATIC)
        install(
            TARGETS zlibstatic
            COMPONENT Development
            EXPORT zlibStaticExport
            RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
            ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
            LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
            PUBLIC_HEADER DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")
        install(
            EXPORT zlibStaticExport
            FILE ZLIB-static.cmake
            NAMESPACE ZLIB::
            DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/zlib)
    endif(ZLIB_BUILD_STATIC)

    configure_package_config_file(
        ${zlib_SOURCE_DIR}/zlibConfig.cmake.in
        ${zlib_BINARY_DIR}/ZLIBConfig.cmake
        INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/zlib)

    write_basic_package_version_file(
        "${zlib_BINARY_DIR}/ZLIBConfigVersion.cmake"
        VERSION "${zlib_VERSION}"
        COMPATIBILITY AnyNewerVersion)

    install(FILES ${zlib_BINARY_DIR}/ZLIBConfig.cmake
                  ${zlib_BINARY_DIR}/ZLIBConfigVersion.cmake
            DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/zlib)
    install(
        FILES zlib.3
        COMPONENT Docs
        DESTINATION "${CMAKE_INSTALL_MANDIR}/man3")
    install(
        FILES LICENSE
              doc/algorithm.txt
              doc/crc-doc.1.0.pdf
              doc/rfc1950.txt
              doc/rfc1951.txt
              doc/rfc1952.txt
              doc/txtvsbin.txt
        COMPONENT Docs
        DESTINATION "${CMAKE_INSTALL_DOCDIR}/zlib")
    install(
        FILES ${ZLIB_PC}
        COMPONENT Development
        DESTINATION "${CMAKE_INSTALL_LIBDIR}/pkgconfig")
endif(ZLIB_INSTALL)

#============================================================================
# Benchmarker
#============================================================================
enable_language(CXX)
set(CMAKE_CXX_STANDARD 14) # workaround for older compilers (e.g. g++ 5.4).
add_executable(zlib_bench contrib/bench/zlib_bench.cc)
target_link_libraries(zlib_bench zlib)

#============================================================================
# Unit Tests
#============================================================================
if (BUILD_UNITTESTS)
    include (ExternalProject)
    set_directory_properties(PROPERTIES EP_PREFIX ${CMAKE_BINARY_DIR}/third_party)
    ExternalProject_add(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG d1467f5813f4d363cfd11aba99c4e9fe47a85e99
        UPDATE_COMMAND ""
        INSTALL_COMMAND ""
        LOG_DOWNLOAD ON
        LOG_CONFIGURE ON
        LOG_BUILD ON
    )

    # gtest includedir
    ExternalProject_Get_Property(googletest source_dir)
    set(GTEST_INCLUDE_DIRS
        ${source_dir}/googletest/include
        ${source_dir}/googletest/include/gtest
    )

    # gtest library
    ExternalProject_Get_Property(googletest binary_dir)
    set(GTEST_LIBRARY_PATH ${binary_dir}/lib/${CMAKE_FIND_LIBRARY_PREFIXES}gtest.a)
    set(GTEST_LIBRARY gtest)
    add_library(${GTEST_LIBRARY} UNKNOWN IMPORTED)
    set_property(TARGET ${GTEST_LIBRARY} PROPERTY IMPORTED_LOCATION ${GTEST_LIBRARY_PATH})
    add_dependencies(${GTEST_LIBRARY} googletest)

    set(UTEST_SRC
        ${CMAKE_CURRENT_SOURCE_DIR}/contrib/tests/infcover.cc
        ${CMAKE_CURRENT_SOURCE_DIR}/contrib/tests/infcover.h
        ${CMAKE_CURRENT_SOURCE_DIR}/contrib/tests/utils_unittest.cc
        ${CMAKE_CURRENT_SOURCE_DIR}/contrib/tests/standalone_test_runner.cc
        ${CMAKE_CURRENT_SOURCE_DIR}/google/compression_utils_portable.cc
    )

    add_compile_definitions(CMAKE_STANDALONE_UNITTESTS)

    add_executable(zlib_unittests ${UTEST_SRC})
    target_include_directories(zlib_unittests PUBLIC ${GTEST_INCLUDE_DIRS})
    target_include_directories(zlib_unittests PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/google)

    target_link_libraries(zlib_unittests ${GTEST_LIBRARY})
    target_link_libraries(zlib_unittests zlib)
    # Needed by gtest
    target_link_libraries(zlib_unittests pthread)
endif()

#============================================================================
# Minizip tool
#============================================================================
# TODO(cavalcantii): get it working on Windows.
if (BUILD_MINIZIP_BIN)
  add_executable(minizip_bin contrib/minizip/minizip.c contrib/minizip/ioapi.c
    contrib/minizip/ioapi.h contrib/minizip/unzip.c
    contrib/minizip/unzip.h contrib/minizip/zip.c contrib/minizip/zip.h
    contrib/minizip/ints.h contrib/minizip/skipset.h
    )
  target_link_libraries(minizip_bin zlib)
endif()

#============================================================================
# zpipe tool
#============================================================================
if (BUILD_ZPIPE)
  add_executable(zpipe examples/zpipe.c)
  target_link_libraries(zpipe zlib)
endif()
#============================================================================
# MiniGzip tool
#============================================================================
if (BUILD_MINIGZIP)
  add_executable(minigzip_bin test/minigzip.c)
  target_link_libraries(minigzip_bin zlib)
endif()
