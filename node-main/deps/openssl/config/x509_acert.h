#if defined(OPENSSL_NO_ASM)
# include "./x509_acert_no-asm.h"
#else
# include "./x509_acert_asm.h"
#endif
