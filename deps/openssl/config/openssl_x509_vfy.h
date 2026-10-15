#if defined(OPENSSL_NO_ASM)
# include "./openssl_x509_vfy_no-asm.h"
#else
# include "./openssl_x509_vfy_asm.h"
#endif
