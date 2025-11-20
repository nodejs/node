#if defined(OPENSSL_NO_ASM)
# include "./x509_vfy_no-asm.h"
#else
# include "./x509_vfy_asm.h"
#endif
