#if defined(OPENSSL_NO_ASM)
# include "./openssl_crypto_no-asm.h"
#else
# include "./openssl_crypto_asm.h"
#endif
