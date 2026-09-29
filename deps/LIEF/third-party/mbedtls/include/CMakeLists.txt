option(INSTALL_MBEDTLS_HEADERS "Install Mbed TLS headers." ON)

if(INSTALL_MBEDTLS_HEADERS)

    file(GLOB headers "mbedtls/*.h")

    install(FILES ${headers}
        DESTINATION include/mbedtls
        PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ)

    file(GLOB private_headers "mbedtls/private/*.h")

    install(FILES ${private_headers}
        DESTINATION include/mbedtls/private
        PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ)
endif(INSTALL_MBEDTLS_HEADERS)

# Make mbedtls_config.h available in an out-of-source build. ssl-opt.sh requires it.
if (ENABLE_TESTING AND NOT ${CMAKE_CURRENT_BINARY_DIR} STREQUAL ${CMAKE_CURRENT_SOURCE_DIR})
    link_to_source(mbedtls)
endif()
