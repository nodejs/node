#include "mbedtls/mbedtls_config.h"

#if defined(MBEDTLS_ARCH_IS_X86)
  #undef MBEDTLS_AESNI_C
#endif

#if defined(MBEDTLS_DEBUG_C)
  #undef MBEDTLS_DEBUG_C
#endif
