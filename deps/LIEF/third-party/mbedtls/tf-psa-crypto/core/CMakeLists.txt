set(src_crypto
    psa_crypto.c
    psa_crypto_client.c
    psa_crypto_driver_wrappers_no_static.c
    psa_crypto_slot_management.c
    psa_crypto_storage.c
    psa_its_file.c
    tf_psa_crypto_config.c
    tf_psa_crypto_version.c
)

if(GEN_FILES)
    add_custom_command(
        OUTPUT
            ${CMAKE_CURRENT_BINARY_DIR}/psa_crypto_driver_wrappers.h
            ${CMAKE_CURRENT_BINARY_DIR}/psa_crypto_driver_wrappers_no_static.c
        COMMAND
            ${TF_PSA_CRYPTO_PYTHON_EXECUTABLE}
                ${TF_PSA_CRYPTO_DIR}/scripts/generate_driver_wrappers.py
                ${CMAKE_CURRENT_BINARY_DIR}
        DEPENDS
            ${TF_PSA_CRYPTO_DIR}/scripts/generate_driver_wrappers.py
            ${TF_PSA_CRYPTO_DIR}/scripts/data_files/driver_templates/psa_crypto_driver_wrappers.h.jinja
            ${TF_PSA_CRYPTO_DIR}/scripts/data_files/driver_templates/psa_crypto_driver_wrappers_no_static.c.jinja
    )

    execute_process(
        COMMAND
            ${TF_PSA_CRYPTO_PYTHON_EXECUTABLE}
            ${TF_PSA_CRYPTO_DIR}/scripts/generate_config_checks.py
            --list-for-cmake "${CMAKE_CURRENT_BINARY_DIR}"
        WORKING_DIRECTORY
            ${CMAKE_CURRENT_SOURCE_DIR}/..
        OUTPUT_VARIABLE
            TF_PSA_CRYPTO_GENERATED_CONFIG_CHECKS_HEADERS)

    add_custom_command(
        OUTPUT ${TF_PSA_CRYPTO_GENERATED_CONFIG_CHECKS_HEADERS}
        COMMAND
            ${TF_PSA_CRYPTO_PYTHON_EXECUTABLE}
                ${TF_PSA_CRYPTO_DIR}/scripts/generate_config_checks.py
                ${CMAKE_CURRENT_BINARY_DIR}
        DEPENDS
            ${TF_PSA_CRYPTO_DIR}/scripts/generate_config_checks.py
            ${TF_PSA_CRYPTO_FRAMEWORK_DIR}/scripts/mbedtls_framework/config_checks_generator.py
    )

    add_custom_target(
            ${TF_PSA_CRYPTO_TARGET_PREFIX}libtfpsacrypto_generated_files_target
        DEPENDS
            "${CMAKE_CURRENT_BINARY_DIR}/psa_crypto_driver_wrappers.h"
            "${CMAKE_CURRENT_BINARY_DIR}/psa_crypto_driver_wrappers_no_static.c"
            ${TF_PSA_CRYPTO_GENERATED_CONFIG_CHECKS_HEADERS}
        )

    # List generated headers as sources explicitly. Normally CMake finds
    # headers by tracing include directives, but if that happens before the
    # generated headers are generated, this process doesn't find them.
    list(APPEND src_crypto
        "${CMAKE_CURRENT_BINARY_DIR}/psa_crypto_driver_wrappers.h"
        ${TF_PSA_CRYPTO_GENERATED_CONFIG_CHECKS_HEADERS}
    )
endif(GEN_FILES)

if(CMAKE_COMPILER_IS_GNUCC)
    set(LIBS_C_FLAGS -Wmissing-declarations -Wmissing-prototypes)
endif(CMAKE_COMPILER_IS_GNUCC)

if(CMAKE_COMPILER_IS_CLANG)
    set(LIBS_C_FLAGS -Wmissing-declarations -Wmissing-prototypes -Wdocumentation -Wno-documentation-deprecated-sync -Wunreachable-code)
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

if(WIN32)
    set(libs ${libs} ws2_32 bcrypt)
endif(WIN32)

if(LINK_WITH_PTHREAD)
    set(libs ${libs} ${CMAKE_THREAD_LIBS_INIT})
endif()

if(LINK_WITH_TRUSTED_STORAGE)
    set(libs ${libs} trusted_storage)
endif()

if (NOT USE_STATIC_TF_PSA_CRYPTO_LIBRARY AND NOT USE_SHARED_TF_PSA_CRYPTO_LIBRARY)
    message(FATAL_ERROR "Need to choose static or shared TF-PSA-Crypto build!")
endif(NOT USE_STATIC_TF_PSA_CRYPTO_LIBRARY AND NOT USE_SHARED_TF_PSA_CRYPTO_LIBRARY)

set(tfpsacrypto_target "${TF_PSA_CRYPTO_TARGET_PREFIX}tfpsacrypto")
set(builtin_target "${TF_PSA_CRYPTO_TARGET_PREFIX}builtin")
set(everest_target "${TF_PSA_CRYPTO_TARGET_PREFIX}everest")
set(p256m_target "${TF_PSA_CRYPTO_TARGET_PREFIX}p256m")

if (USE_STATIC_TF_PSA_CRYPTO_LIBRARY)
    set(tfpsacrypto_static_target ${tfpsacrypto_target})
    set(builtin_static_target ${builtin_target})
    set(everest_static_target ${everest_target})
    set(p256m_static_target ${p256m_target})
endif()

set(target_libraries ${tfpsacrypto_target})

if(USE_STATIC_TF_PSA_CRYPTO_LIBRARY AND USE_SHARED_TF_PSA_CRYPTO_LIBRARY)
    string(APPEND tfpsacrypto_static_target "_static")
    string(APPEND builtin_static_target "_static")
    string(APPEND everest_static_target "_static")
    string(APPEND p256m_static_target "_static")

    list(APPEND target_libraries
        ${tfpsacrypto_static_target})
endif()

if(USE_STATIC_TF_PSA_CRYPTO_LIBRARY)
    add_library(${tfpsacrypto_static_target} STATIC
        ${src_crypto}
        $<TARGET_OBJECTS:${builtin_static_target}>
        $<TARGET_OBJECTS:${everest_static_target}>
        $<TARGET_OBJECTS:${p256m_static_target}>)
    tf_psa_crypto_set_base_compile_options(${tfpsacrypto_static_target})
    target_compile_options(${tfpsacrypto_static_target} PRIVATE ${LIBS_C_FLAGS})
    set_target_properties(${tfpsacrypto_static_target} PROPERTIES OUTPUT_NAME tfpsacrypto)
    target_link_libraries(${tfpsacrypto_static_target} PUBLIC ${libs})

    if(GEN_FILES)
        add_dependencies(${tfpsacrypto_static_target}
            ${TF_PSA_CRYPTO_TARGET_PREFIX}libtfpsacrypto_generated_files_target)
    endif()
endif(USE_STATIC_TF_PSA_CRYPTO_LIBRARY)

if(USE_SHARED_TF_PSA_CRYPTO_LIBRARY)
    set(CMAKE_LIBRARY_PATH ${CMAKE_CURRENT_BINARY_DIR})
    add_library(${tfpsacrypto_target} SHARED
        ${src_crypto} $<TARGET_OBJECTS:${builtin_target}>
        ${src_crypto} $<TARGET_OBJECTS:${everest_target}>
        ${src_crypto} $<TARGET_OBJECTS:${p256m_target}>)
    tf_psa_crypto_set_base_compile_options(${tfpsacrypto_target})
    target_compile_options(${tfpsacrypto_target} PRIVATE ${LIBS_C_FLAGS})
    set_target_properties(${tfpsacrypto_target} PROPERTIES VERSION ${TF_PSA_CRYPTO_VERSION} SOVERSION ${TF_PSA_CRYPTO_SOVERSION})
    target_link_libraries(${tfpsacrypto_target} PUBLIC ${libs})

    if(GEN_FILES)
        add_dependencies(${tfpsacrypto_target}
            ${TF_PSA_CRYPTO_TARGET_PREFIX}libtfpsacrypto_generated_files_target)
    endif()
endif(USE_SHARED_TF_PSA_CRYPTO_LIBRARY)

foreach(target IN LISTS target_libraries)
    add_library(TF-PSA-Crypto::${target} ALIAS ${target})  # add_subdirectory support
    target_include_directories(${target}
        PUBLIC $<BUILD_INTERFACE:${TF_PSA_CRYPTO_DIR}/include/>
               $<BUILD_INTERFACE:${TF_PSA_CRYPTO_DIR}/drivers/builtin/include/>
               $<BUILD_INTERFACE:${TF_PSA_CRYPTO_DIR}/drivers/everest/include/>
               $<BUILD_INTERFACE:${TF_PSA_CRYPTO_DIR}/drivers/p256-m/>
               $<INSTALL_INTERFACE:include/>
        PRIVATE ${TF_PSA_CRYPTO_DIR}/core
                ${TF_PSA_CRYPTO_DIR}/drivers/builtin/src
                # Needed to include psa_crypto_driver_wrappers.h
                ${CMAKE_CURRENT_BINARY_DIR})
    tf_psa_crypto_set_config_files_compile_definitions(${target})
    install(
        TARGETS ${target}
        EXPORT MbedTLSTargets
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ)
    install(
        TARGETS ${target}
        EXPORT TF-PSA-CryptoTargets
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ)
endforeach(target)
