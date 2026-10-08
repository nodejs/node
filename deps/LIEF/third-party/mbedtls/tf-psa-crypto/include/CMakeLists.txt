option(INSTALL_TF_PSA_CRYPTO_HEADERS "Install TF PSA Crypto headers." ON)

if(INSTALL_TF_PSA_CRYPTO_HEADERS)
    file(GLOB psa_headers "psa/*.h")
    file(GLOB tf-psa-crypto_headers "tf-psa-crypto/*.h")
    file(GLOB mbedtls_crypto_headers "../drivers/builtin/include/mbedtls/*.h" "mbedtls/*.h")

    install(FILES ${psa_headers}
        DESTINATION include/psa
        PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ)

    install(FILES ${tf-psa-crypto_headers}
        DESTINATION include/tf-psa-crypto
        PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ)

    install(FILES ${mbedtls_crypto_headers}
        DESTINATION include/mbedtls
        PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ)
endif(INSTALL_TF_PSA_CRYPTO_HEADERS)

# Make includes available in an out-of-source build. ssl-opt.sh requires it.
if (ENABLE_TESTING AND NOT ${CMAKE_CURRENT_BINARY_DIR} STREQUAL ${CMAKE_CURRENT_SOURCE_DIR})
    tf_psa_crypto_link_to_source(psa)
endif()
