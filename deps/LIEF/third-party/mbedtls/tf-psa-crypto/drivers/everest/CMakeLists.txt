set(everest_target "${TF_PSA_CRYPTO_TARGET_PREFIX}everest")
if (USE_STATIC_TF_PSA_CRYPTO_LIBRARY)
    set(everest_static_target ${everest_target})
endif()
set(target_libraries ${everest_target})
if(USE_STATIC_TF_PSA_CRYPTO_LIBRARY AND USE_SHARED_TF_PSA_CRYPTO_LIBRARY)
    string(APPEND everest_static_target "_static")
    list(APPEND target_libraries ${everest_static_target})
endif()

foreach (target IN LISTS target_libraries)
    add_library(${target} OBJECT
      library/everest.c
      library/x25519.c
      library/Hacl_Curve25519_joined.c)

    tf_psa_crypto_set_base_compile_options(${target})
    target_include_directories(${target}
      PRIVATE include
              ${TF_PSA_CRYPTO_DIR}/include
              ${TF_PSA_CRYPTO_DIR}/drivers/builtin/include
              include/tf-psa-crypto/private/everest
              include/tf-psa-crypto/private/everest/kremlib
              ${TF_PSA_CRYPTO_DIR}/core)
    tf_psa_crypto_set_config_files_compile_definitions(${target})
endforeach(target)

if(USE_SHARED_TF_PSA_CRYPTO_LIBRARY)
    set_property(TARGET ${everest_target} PROPERTY POSITION_INDEPENDENT_CODE ON)
endif(USE_SHARED_TF_PSA_CRYPTO_LIBRARY)

if(INSTALL_TF_PSA_CRYPTO_HEADERS)

  install(DIRECTORY include/tf-psa-crypto/private/everest
    DESTINATION include/tf-psa-crypto/private/everest
    FILE_PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ
    DIRECTORY_PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
    FILES_MATCHING PATTERN "*.h")

endif(INSTALL_TF_PSA_CRYPTO_HEADERS)
