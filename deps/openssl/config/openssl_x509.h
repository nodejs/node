#if defined(OPENSSL_NO_ASM)
# include "./openssl_x509_no-asm.h"
#else
# include "./openssl_x509_asm.h"
#endif
