set(src_x509
    error.c
    mbedtls_config.c
    pkcs7.c
    x509.c
    x509_create.c
    x509_crl.c
    x509_crt.c
    x509_csr.c
    x509_oid.c
    x509write.c
    x509write_crt.c
    x509write_csr.c
)

set(src_tls
    debug.c
    mps_reader.c
    mps_trace.c
    net_sockets.c
    ssl_cache.c
    ssl_ciphersuites.c
    ssl_client.c
    ssl_cookie.c
    ssl_debug_helpers_generated.c
    ssl_msg.c
    ssl_ticket.c
    ssl_tls.c
    ssl_tls12_client.c
    ssl_tls12_server.c
    ssl_tls13_keys.c
    ssl_tls13_server.c
    ssl_tls13_client.c
    ssl_tls13_generic.c
    timing.c
    version.c
    version_features.c
)

if(GEN_FILES)
    find_package(Perl REQUIRED)

    file(GLOB crypto_error_headers ${CMAKE_CURRENT_SOURCE_DIR}/include/mbedtls/*.h)
    file(GLOB tls_error_headers ${MBEDTLS_DIR}/include/mbedtls/*.h)
    add_custom_command(
        OUTPUT
            ${CMAKE_CURRENT_BINARY_DIR}/error.c
        COMMAND
            ${PERL_EXECUTABLE}
                ${CMAKE_CURRENT_SOURCE_DIR}/../scripts/generate_errors.pl
                ${CMAKE_CURRENT_SOURCE_DIR}/../tf-psa-crypto/drivers/builtin/include/mbedtls
                ${CMAKE_CURRENT_SOURCE_DIR}/../include/mbedtls
                ${CMAKE_CURRENT_SOURCE_DIR}/../scripts/data_files
                ${CMAKE_CURRENT_BINARY_DIR}/${TF_PSA_CRYPTO_DRIVERS_BUILTIN_SRC_DIR}/error.c
        DEPENDS
            ${MBEDTLS_DIR}/scripts/generate_errors.pl
            ${crypto_error_headers}
            ${tls_error_headers}
            ${MBEDTLS_DIR}/scripts/data_files/error.fmt
    )
    add_custom_command(
        OUTPUT
            ${CMAKE_CURRENT_BINARY_DIR}/version_features.c
        COMMAND
            ${PERL_EXECUTABLE}
                ${CMAKE_CURRENT_SOURCE_DIR}/../scripts/generate_features.pl
                ${CMAKE_CURRENT_SOURCE_DIR}/../include/mbedtls
                ${CMAKE_CURRENT_SOURCE_DIR}/../scripts/data_files
                ${CMAKE_CURRENT_BINARY_DIR}/version_features.c
        DEPENDS
            ${CMAKE_CURRENT_SOURCE_DIR}/../scripts/generate_features.pl
            ${CMAKE_CURRENT_SOURCE_DIR}/../include/mbedtls/mbedtls_config.h
            ${CMAKE_CURRENT_SOURCE_DIR}/../scripts/data_files/version_features.fmt
    )

    execute_process(
        COMMAND
            ${MBEDTLS_PYTHON_EXECUTABLE}
            ${MBEDTLS_DIR}/scripts/generate_config_checks.py
            --list-for-cmake "${CMAKE_CURRENT_BINARY_DIR}"
        WORKING_DIRECTORY
            ${CMAKE_CURRENT_SOURCE_DIR}/..
        OUTPUT_VARIABLE
            MBEDTLS_GENERATED_CONFIG_CHECKS_HEADERS)

    add_custom_command(
        OUTPUT ${MBEDTLS_GENERATED_CONFIG_CHECKS_HEADERS}
        COMMAND
            ${MBEDTLS_PYTHON_EXECUTABLE}
                ${MBEDTLS_DIR}/scripts/generate_config_checks.py
                ${CMAKE_CURRENT_BINARY_DIR}
        DEPENDS
            ${MBEDTLS_DIR}/scripts/generate_config_checks.py
            ${MBEDTLS_FRAMEWORK_DIR}/scripts/mbedtls_framework/config_checks_generator.py
    )

    add_custom_command(
        OUTPUT
            ${CMAKE_CURRENT_BINARY_DIR}/ssl_debug_helpers_generated.c
        COMMAND
            ${MBEDTLS_PYTHON_EXECUTABLE}
                ${CMAKE_CURRENT_SOURCE_DIR}/../framework/scripts/generate_ssl_debug_helpers.py
                --mbedtls-root ${CMAKE_CURRENT_SOURCE_DIR}/..
                ${CMAKE_CURRENT_BINARY_DIR}
        DEPENDS
            ${CMAKE_CURRENT_SOURCE_DIR}/../framework/scripts/generate_ssl_debug_helpers.py
            ${tls_error_headers}
    )

    add_custom_target(${MBEDTLS_TARGET_PREFIX}libmbedx509_generated_files_target
        DEPENDS
            ${CMAKE_CURRENT_BINARY_DIR}/error.c
            ${MBEDTLS_GENERATED_CONFIG_CHECKS_HEADERS}
    )

    add_custom_target(${MBEDTLS_TARGET_PREFIX}libmbedtls_generated_files_target
        DEPENDS
            ${CMAKE_CURRENT_BINARY_DIR}/ssl_debug_helpers_generated.c
            ${CMAKE_CURRENT_BINARY_DIR}/version_features.c
    )

    # List generated headers as sources explicitly. Normally CMake finds
    # headers by tracing include directives, but if that happens before the
    # generated headers are generated, this process doesn't find them.
    list(APPEND src_x509
        ${MBEDTLS_GENERATED_CONFIG_CHECKS_HEADERS}
    )
endif()

if(CMAKE_COMPILER_IS_GNUCC)
    set(LIBS_C_FLAGS -Wmissing-declarations)
endif(CMAKE_COMPILER_IS_GNUCC)

if(CMAKE_COMPILER_IS_CLANG)
    set(LIBS_C_FLAGS -Wmissing-declarations -Wdocumentation -Wno-documentation-deprecated-sync -Wunreachable-code)
endif(CMAKE_COMPILER_IS_CLANG)

if(CMAKE_COMPILER_IS_MSVC)
    option(MSVC_STATIC_RUNTIME "Build the libraries with /MT compiler flag" OFF)
    if(MSVC_STATIC_RUNTIME)
        foreach(flag_var
            CMAKE_C_FLAGS CMAKE_C_FLAGS_DEBUG CMAKE_C_FLAGS_RELEASE
            CMAKE_C_FLAGS_MINSIZEREL CMAKE_C_FLAGS_RELWITHDEBINFO
            CMAKE_C_FLAGS_CHECK)
            string(REGEX REPLACE "/MD" "/MT" ${flag_var} "${${flag_var}}")
        endforeach(flag_var)
    endif()
endif()

if(CMAKE_C_COMPILER_ID MATCHES "AppleClang")
    set(CMAKE_C_ARCHIVE_CREATE   "<CMAKE_AR> Scr <TARGET> <LINK_FLAGS> <OBJECTS>")
    set(CMAKE_C_ARCHIVE_FINISH   "<CMAKE_RANLIB> -no_warning_for_no_symbols -c <TARGET>")
endif()
if(CMAKE_CXX_COMPILER_ID MATCHES "AppleClang")
    set(CMAKE_CXX_ARCHIVE_CREATE "<CMAKE_AR> Scr <TARGET> <LINK_FLAGS> <OBJECTS>")
    set(CMAKE_CXX_ARCHIVE_FINISH "<CMAKE_RANLIB> -no_warning_for_no_symbols -c <TARGET>")
endif()

if(HAIKU)
    set(libs ${libs} network)
endif(HAIKU)

if(LINK_WITH_PTHREAD)
    set(libs ${libs} ${CMAKE_THREAD_LIBS_INIT})
endif()

if (NOT USE_STATIC_MBEDTLS_LIBRARY AND NOT USE_SHARED_MBEDTLS_LIBRARY)
    message(FATAL_ERROR "Need to choose static or shared mbedtls build!")
endif(NOT USE_STATIC_MBEDTLS_LIBRARY AND NOT USE_SHARED_MBEDTLS_LIBRARY)

set(mbedtls_target    "${MBEDTLS_TARGET_PREFIX}mbedtls")
set(mbedx509_target   "${MBEDTLS_TARGET_PREFIX}mbedx509")

set(mbedtls_target    ${mbedtls_target}    PARENT_SCOPE)
set(mbedx509_target   ${mbedx509_target}   PARENT_SCOPE)

if (USE_STATIC_MBEDTLS_LIBRARY)
    set(mbedtls_static_target  ${mbedtls_target})
    set(mbedx509_static_target ${mbedx509_target})
endif()

set(target_libraries ${mbedx509_target} ${mbedtls_target})

if(USE_STATIC_MBEDTLS_LIBRARY AND USE_SHARED_MBEDTLS_LIBRARY)
    string(APPEND mbedtls_static_target    "_static")
    string(APPEND mbedx509_static_target   "_static")

    list(APPEND target_libraries
        ${mbedx509_static_target}
        ${mbedtls_static_target})
endif()

if(USE_STATIC_MBEDTLS_LIBRARY)
    add_library(${mbedx509_static_target} STATIC ${src_x509})
    set_base_compile_options(${mbedx509_static_target})
    target_compile_options(${mbedx509_static_target} PRIVATE ${LIBS_C_FLAGS})
    set_target_properties(${mbedx509_static_target} PROPERTIES OUTPUT_NAME mbedx509)
    target_link_libraries(${mbedx509_static_target} PUBLIC ${libs} ${tfpsacrypto_static_target})

    add_library(${mbedtls_static_target} STATIC ${src_tls})
    set_base_compile_options(${mbedtls_static_target})
    target_compile_options(${mbedtls_static_target} PRIVATE ${LIBS_C_FLAGS})
    set_target_properties(${mbedtls_static_target} PROPERTIES OUTPUT_NAME mbedtls)
    target_link_libraries(${mbedtls_static_target} PUBLIC ${libs} ${mbedx509_static_target})

    if(GEN_FILES)
        add_dependencies(${mbedx509_static_target}
            ${MBEDTLS_TARGET_PREFIX}libmbedx509_generated_files_target)
        add_dependencies(${mbedtls_static_target}
            ${MBEDTLS_TARGET_PREFIX}libmbedtls_generated_files_target)
    endif()
endif(USE_STATIC_MBEDTLS_LIBRARY)

if(USE_SHARED_MBEDTLS_LIBRARY)
    add_library(${mbedx509_target} SHARED ${src_x509})
    set_base_compile_options(${mbedx509_target})
    target_compile_options(${mbedx509_target} PRIVATE ${LIBS_C_FLAGS})
    set_target_properties(${mbedx509_target} PROPERTIES VERSION ${MBEDTLS_VERSION} SOVERSION ${MBEDTLS_X509_SOVERSION})
    target_link_libraries(${mbedx509_target} PUBLIC ${libs} ${tfpsacrypto_target})

    add_library(${mbedtls_target} SHARED ${src_tls})
    set_base_compile_options(${mbedtls_target})
    target_compile_options(${mbedtls_target} PRIVATE ${LIBS_C_FLAGS})
    set_target_properties(${mbedtls_target} PROPERTIES VERSION ${MBEDTLS_VERSION} SOVERSION ${MBEDTLS_TLS_SOVERSION})
    target_link_libraries(${mbedtls_target} PUBLIC ${libs} ${mbedx509_target})

    if(GEN_FILES)
        add_dependencies(${mbedx509_target}
            ${MBEDTLS_TARGET_PREFIX}libmbedx509_generated_files_target)
        add_dependencies(${mbedtls_target}
            ${MBEDTLS_TARGET_PREFIX}libmbedtls_generated_files_target)
    endif()
endif(USE_SHARED_MBEDTLS_LIBRARY)

foreach(target IN LISTS target_libraries)
    add_library(MbedTLS::${target} ALIAS ${target})  # add_subdirectory support
    # Include public header files from /include, /tf-psa-crypto/include/ and
    # tf-psa-crypto/drivers/builtin/include/. Include private header files
    # from /library, tf-psa-crypto/core/ and tf-psa-crypto/drivers/builtin/src/.
    target_include_directories(${target}
        PUBLIC $<BUILD_INTERFACE:${MBEDTLS_DIR}/include/>
               $<BUILD_INTERFACE:${MBEDTLS_DIR}/tf-psa-crypto/include/>
               $<BUILD_INTERFACE:${MBEDTLS_DIR}/tf-psa-crypto/drivers/builtin/include/>
               $<INSTALL_INTERFACE:include/>
        PRIVATE ${MBEDTLS_DIR}/library/
                ${MBEDTLS_DIR}/tf-psa-crypto/core
                ${MBEDTLS_DIR}/tf-psa-crypto/drivers/builtin/src
                # needed for generated headers
                ${CMAKE_CURRENT_BINARY_DIR})
    set_config_files_compile_definitions(${target})
    install(
        TARGETS ${target}
        EXPORT MbedTLSTargets
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ)
endforeach(target)

set(lib_target "${MBEDTLS_TARGET_PREFIX}lib")

add_custom_target(${lib_target} DEPENDS ${mbedx509_target} ${mbedtls_target})
if(USE_STATIC_MBEDTLS_LIBRARY AND USE_SHARED_MBEDTLS_LIBRARY)
    add_dependencies(${lib_target} ${mbedx509_static_target} ${mbedtls_static_target})
endif()

foreach(target IN LISTS tf_psa_crypto_library_targets)
    get_target_property(target_type ${target} TYPE)
    if (target_type STREQUAL STATIC_LIBRARY)
        add_custom_command(
            TARGET ${mbedtls_target} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    $<TARGET_FILE:${target}>
                    $<TARGET_FILE_NAME:${target}>
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    $<TARGET_FILE:${target}>
                    "libmbedcrypto.a"
        )
        install(FILES $<TARGET_FILE:${target}>
                DESTINATION ${CMAKE_INSTALL_LIBDIR}
                RENAME "libmbedcrypto.a"
        )
    else()
        # Copy the crypto shared library from tf-psa-crypto:
        # - ".so.<VERSION>" on Unix
        # - ".dylib" on macOS
        # - ".dll" on Windows
        # The full path to the file is given by $<TARGET_FILE:${target}>.
        #
        # On systems that use .so versioning, also create the symbolic links
        # ".so.<SOVERSION>" and ".so", which correspond to
        # $<TARGET_SONAME_FILE_NAME:${target}> and $<TARGET_LINKER_FILE_NAME:${target}>,
        # respectively.
        #
        # On Windows, also copy the ".lib" file, whose full path is
        # $<TARGET_LINKER_FILE:${target}>.
        #
        # Provide also the crypto libraries under their historical names:
        # "libmbedcrypto.*"
        add_custom_command(
            TARGET ${mbedtls_target} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    $<TARGET_FILE:${target}>
                    $<TARGET_FILE_NAME:${target}>
        )
        if(APPLE)
            add_custom_command(
                TARGET ${mbedtls_target} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E create_symlink
                $<TARGET_FILE_NAME:${target}>
                libmbedcrypto.dylib
            )
            install(FILES $<TARGET_FILE:${target}>
                    DESTINATION ${CMAKE_INSTALL_LIBDIR}
                    RENAME "libmbedcrypto.dylib"
            )
        elseif(WIN32 AND NOT CYGWIN)
            add_custom_command(
                TARGET ${mbedtls_target} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                        $<TARGET_FILE:${target}>
                        libmbedcrypto.dll
            )
            add_custom_command(
                TARGET ${mbedtls_target} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                        $<TARGET_LINKER_FILE:${target}>
                        $<TARGET_LINKER_FILE_NAME:${target}>
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                        $<TARGET_LINKER_FILE:${target}>
                        libmbedcrypto.lib
            )
            install(FILES $<TARGET_FILE:${target}>
                    DESTINATION ${CMAKE_INSTALL_BINDIR}
                    RENAME "libmbedcrypto.dll"
            )
            install(FILES $<TARGET_LINKER_FILE:${target}>
                    DESTINATION ${CMAKE_INSTALL_LIBDIR}
                    RENAME "libmbedcrypto.lib"
            )
        else()
            add_custom_command(
              TARGET ${mbedtls_target} POST_BUILD
              COMMAND ${CMAKE_COMMAND} -E create_symlink
                      $<TARGET_FILE_NAME:${target}>
                      $<TARGET_SONAME_FILE_NAME:${target}>
              COMMAND ${CMAKE_COMMAND} -E create_symlink
                      $<TARGET_SONAME_FILE_NAME:${target}>
                      $<TARGET_LINKER_FILE_NAME:${target}>
              COMMAND ${CMAKE_COMMAND} -E create_symlink
                      $<TARGET_FILE_NAME:${target}>
                      libmbedcrypto.so.${MBEDTLS_VERSION}
              COMMAND ${CMAKE_COMMAND} -E create_symlink
                      libmbedcrypto.so.${MBEDTLS_VERSION}
                      libmbedcrypto.so.${MBEDTLS_CRYPTO_SOVERSION}
              COMMAND ${CMAKE_COMMAND} -E create_symlink
                      libmbedcrypto.so.${MBEDTLS_CRYPTO_SOVERSION}
                      libmbedcrypto.so
            )
            install(FILES $<TARGET_FILE:${target}>
                    DESTINATION ${CMAKE_INSTALL_LIBDIR}
                    RENAME "libmbedcrypto.so.${MBEDTLS_VERSION}"
            )
            install(CODE "
                set(_libdir \"\${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}\")

                execute_process(COMMAND \"\${CMAKE_COMMAND}\" -E create_symlink
                                \"libmbedcrypto.so.${MBEDTLS_VERSION}\"
                                \${_libdir}/libmbedcrypto.so.${MBEDTLS_CRYPTO_SOVERSION})
                execute_process(COMMAND \"\${CMAKE_COMMAND}\" -E create_symlink
                                \"libmbedcrypto.so.${MBEDTLS_CRYPTO_SOVERSION}\"
                                \${_libdir}/libmbedcrypto.so)
            ")
        endif()
    endif()
endforeach(target)
