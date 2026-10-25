// Disable MbedTLS options that are not thread-safe. See
// `tests/scripts/components-sanitizers.sh` in the MbedTLS tree: both options
// are unset there when building with MBEDTLS_THREADING_PTHREAD under TSan.
#if defined(MBEDTLS_THREADING_C)
  // Self-tests do not currently use multiple threads.
  #if defined(MBEDTLS_SELF_TEST)
    #undef MBEDTLS_SELF_TEST
  #endif

  // Interruptible ECC operations are not thread-safe.
  #if defined(MBEDTLS_ECP_RESTARTABLE)
    #undef MBEDTLS_ECP_RESTARTABLE
  #endif
#endif
