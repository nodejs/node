# Written in 2016-2017, 2021 by Henrik Steffen Gaßmann henrik@gassmann.onl
#
# To the extent possible under law, the author(s) have dedicated all
# copyright and related and neighboring rights to this software to the
# public domain worldwide. This software is distributed without any warranty.
#
# You should have received a copy of the CC0 Public Domain Dedication
# along with this software. If not, see
#
#     http://creativecommons.org/publicdomain/zero/1.0/
#
########################################################################
cmake_minimum_required(VERSION 3.10.2)

# new dependent option syntax. We are already compliant
if (POLICY CMP0127)
    cmake_policy(SET CMP0127 NEW)
endif()

project(base64 LANGUAGES C VERSION 0.5.2)

include(GNUInstallDirs)
include(CMakeDependentOption)
include(CheckIncludeFile)
include(FeatureSummary)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake/Modules")

#######################################################################
# platform detection
include(TargetArch)
detect_target_architecture(_TARGET_ARCH)

check_include_file(getopt.h HAVE_GETOPT_H)
cmake_dependent_option(BASE64_BUILD_CLI "Build the cli for encoding and decoding" ON "HAVE_GETOPT_H" OFF)
add_feature_info(CLI BASE64_BUILD_CLI "enables the CLI executable for encoding and decoding")

###################################################################
# optional/conditional dependencies
find_package(OpenMP)
set_package_properties(OpenMP PROPERTIES
    TYPE OPTIONAL
    PURPOSE "Allows to utilize OpenMP"
)


########################################################################
# Compilation options
option(BASE64_WERROR "Treat warnings as error" ON)
option(BASE64_BUILD_TESTS "add test projects" OFF)
cmake_dependent_option(BASE64_WITH_OpenMP "use OpenMP" OFF "OpenMP_FOUND" OFF)
add_feature_info("OpenMP codec" BASE64_WITH_OpenMP "spreads codec work accross multiple threads")
cmake_dependent_option(BASE64_REGENERATE_TABLES "regenerate the codec tables" OFF "NOT CMAKE_CROSSCOMPILING" OFF)

set(_IS_X86 "_TARGET_ARCH_x86 OR _TARGET_ARCH_x64")
cmake_dependent_option(BASE64_WITH_SSSE3 "add SSSE 3 codepath" ON ${_IS_X86} OFF)
add_feature_info(SSSE3 BASE64_WITH_SSSE3 "add SSSE 3 codepath")
cmake_dependent_option(BASE64_WITH_SSE41 "add SSE 4.1 codepath" ON ${_IS_X86} OFF)
add_feature_info(SSE4.1 BASE64_WITH_SSE41 "add SSE 4.1 codepath")
cmake_dependent_option(BASE64_WITH_SSE42 "add SSE 4.2 codepath" ON ${_IS_X86} OFF)
add_feature_info(SSE4.2 BASE64_WITH_SSE42 "add SSE 4.2 codepath")
cmake_dependent_option(BASE64_WITH_AVX "add AVX codepath" ON ${_IS_X86} OFF)
add_feature_info(AVX BASE64_WITH_AVX "add AVX codepath")
cmake_dependent_option(BASE64_WITH_AVX2 "add AVX 2 codepath" ON ${_IS_X86} OFF)
add_feature_info(AVX2 BASE64_WITH_AVX2 "add AVX2 codepath")
cmake_dependent_option(BASE64_WITH_AVX512 "add AVX 512 codepath" ON ${_IS_X86} OFF)
add_feature_info(AVX512 BASE64_WITH_AVX512 "add AVX512 codepath")

cmake_dependent_option(BASE64_WITH_NEON32 "add NEON32 codepath" OFF _TARGET_ARCH_arm OFF)
add_feature_info(NEON32 BASE64_WITH_NEON32 "add NEON32 codepath")

cmake_dependent_option(BASE64_WITH_NEON64 "add NEON64 codepath" ON _TARGET_ARCH_arm64 OFF)
add_feature_info(NEON64 BASE64_WITH_NEON64 "add NEON64 codepath")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/bin")

########################################################################
# Regenerate headers

if (BASE64_REGENERATE_TABLES)
    # Generate tables in build folder and copy to source tree.
    # Don't add the tables in the source tree to the outputs, to avoid `make clean` removing them.
    add_executable(table_generator
        lib/tables/table_generator.c
    )

    add_custom_command(OUTPUT table_dec_32bit.h "${CMAKE_CURRENT_SOURCE_DIR}/lib/tables/table_dec_32bit.h"
        COMMAND table_generator > table_dec_32bit.h
        COMMAND "${CMAKE_COMMAND}" -E copy table_dec_32bit.h "${CMAKE_CURRENT_SOURCE_DIR}/lib/tables/table_dec_32bit.h"
        DEPENDS table_generator
    )
    set(Python_ADDITIONAL_VERSIONS 3)
    find_package(PythonInterp REQUIRED)
    add_custom_command(OUTPUT table_enc_12bit.h "${CMAKE_CURRENT_SOURCE_DIR}/lib/tables/table_enc_12bit.h"
        COMMAND "${PYTHON_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/lib/tables/table_enc_12bit.py" > table_enc_12bit.h
        COMMAND "${CMAKE_COMMAND}" -E copy table_enc_12bit.h "${CMAKE_CURRENT_SOURCE_DIR}/lib/tables/table_enc_12bit.h"
        DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/lib/tables/table_enc_12bit.py"
    )
endif()


########################################################################
# library project
add_library(base64
    # library files
    lib/lib.c
    lib/codec_choose.c
    include/libbase64.h

    lib/tables/tables.c
    # Add generated headers explicitly to target, to insert them in the dependency tree
    lib/tables/table_dec_32bit.h
    lib/tables/table_enc_12bit.h

    # codec implementations
    lib/arch/generic/codec.c

    lib/arch/ssse3/codec.c
    lib/arch/sse41/codec.c
    lib/arch/sse42/codec.c
    lib/arch/avx/codec.c
    lib/arch/avx2/codec.c
    lib/arch/avx512/codec.c

    lib/arch/neon32/codec.c
    lib/arch/neon64/codec.c
)

target_include_directories(base64
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    PRIVATE
        "${CMAKE_CURRENT_BINARY_DIR}"
)

####################################################################
# platform/compiler specific configuration
set_target_properties(base64 PROPERTIES
    C_STANDARD 99
    C_STANDARD_REQUIRED YES
    C_EXTENSIONS OFF
    DEFINE_SYMBOL BASE64_EXPORTS
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
)

#generate_export_header(base64)
# the following definitions and those in libbase64.h have been
# kept forward compatible in case we ever switch to generate_export_header
if (BUILD_SHARED_LIBS)
    set_target_properties(base64 PROPERTIES
        C_VISIBILITY_PRESET hidden
    )
else()
    target_compile_definitions(base64
        PUBLIC
            BASE64_STATIC_DEFINE
    )
endif()

target_compile_options(base64 PRIVATE
  $<$<C_COMPILER_ID:MSVC>:
    /W4
    /we4013 # Error warning C4013: 'function' undefined; assuming extern returning int
    /we4700 # Error warning C4700: uninitialized local variable
    /we4715 # not all control paths return a value
    /we4003 # not enough actual parameters for macro
    /wd4456 # disable warning C4456: declaration of 'xxx' hides previous local declaration
  >
  $<$<NOT:$<C_COMPILER_ID:MSVC>>:
    -Wall
    -Wextra
    -Wpedantic
  >
  $<$<BOOL:${BASE64_WERROR}>:$<IF:$<C_COMPILER_ID:MSVC>,/WX,-Werror>>
)

target_compile_definitions(base64 PRIVATE
  $<$<C_COMPILER_ID:MSVC>:
    # remove unnecessary warnings about unchecked iterators
    _SCL_SECURE_NO_WARNINGS
  >
)

########################################################################
# SIMD settings
include(TargetSIMDInstructionSet)
define_SIMD_compile_flags()

if (_TARGET_ARCH STREQUAL "x86" OR _TARGET_ARCH STREQUAL "x64")
    macro(configure_codec _TYPE)
        if (BASE64_WITH_${_TYPE})
            string(TOLOWER "${_TYPE}" _DIR)
            set_source_files_properties("lib/arch/${_DIR}/codec.c" PROPERTIES
                COMPILE_FLAGS "${COMPILE_FLAGS_${_TYPE}}"
            )

            if (${ARGC} GREATER 1 AND MSVC)
                set_source_files_properties("lib/arch/${_DIR}/codec.c" PROPERTIES
                    COMPILE_DEFINITIONS ${ARGV1}
                )
            endif()
        endif()
    endmacro()

    configure_codec(SSSE3 __SSSE3__)
    configure_codec(SSE41 __SSSE4_1__)
    configure_codec(SSE42 __SSSE4_2__)
    configure_codec(AVX)
    configure_codec(AVX2)
    configure_codec(AVX512)

elseif (_TARGET_ARCH STREQUAL "arm")
    set(BASE64_NEON32_CFLAGS "${COMPILE_FLAGS_NEON32}" CACHE STRING "the NEON32 compile flags (for 'lib/arch/neon32/codec.c')")
    mark_as_advanced(BASE64_NEON32_CFLAGS)

    if (BASE64_WITH_NEON32)
        set_source_files_properties("lib/arch/neon32/codec.c" PROPERTIES
            COMPILE_FLAGS "${BASE64_NEON32_CFLAGS} "
        )
    endif()

#elseif (_TARGET_ARCH STREQUAL "arm64" AND BASE64_WITH_NEON64)

endif()

configure_file("${CMAKE_CURRENT_LIST_DIR}/cmake/config.h.in" "${CMAKE_CURRENT_BINARY_DIR}/config.h" @ONLY)

########################################################################
# OpenMP Settings
if (BASE64_WITH_OpenMP)
    target_link_libraries(base64 PRIVATE OpenMP::OpenMP_C)
endif()

########################################################################
if (BASE64_BUILD_TESTS)
    enable_testing()
    add_subdirectory(test)
endif()

########################################################################
# base64
if (BASE64_BUILD_CLI)
    add_executable(base64-bin
        bin/base64.c
    )
    target_link_libraries(base64-bin PRIVATE base64)
    set_target_properties(base64-bin PROPERTIES
        OUTPUT_NAME base64
    )
endif()

########################################################################
# cmake install
install(DIRECTORY include/ TYPE INCLUDE)
install(TARGETS base64
    EXPORT base64-targets
    DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
if (BASE64_BUILD_CLI)
    install(TARGETS base64-bin EXPORT base64-targets DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()

include(CMakePackageConfigHelpers)
configure_package_config_file(cmake/base64-config.cmake.in
    "${CMAKE_CURRENT_BINARY_DIR}/base64-config.cmake"

    INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}"
)
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/base64-config-version.cmake"
    VERSION ${BASE64_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(FILES
        "${CMAKE_CURRENT_BINARY_DIR}/base64-config.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/base64-config-version.cmake"
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}"
)

install(EXPORT base64-targets
    NAMESPACE aklomp::
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}"
)

########################################################################
feature_summary(WHAT PACKAGES_FOUND PACKAGES_NOT_FOUND ENABLED_FEATURES DISABLED_FEATURES)
