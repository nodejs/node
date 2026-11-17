#if defined(OPENSSL_NO_ASM)
# include "./openssl_ssl_no-asm.h"
#else
# include "./openssl_ssl_asm.h"
#endif
