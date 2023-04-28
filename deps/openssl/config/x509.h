#if defined(OPENSSL_NO_ASM)
# include "./x509_no-asm.h"
#else
# include "./x509_asm.h"
#endif
