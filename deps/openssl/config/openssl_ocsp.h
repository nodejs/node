#if defined(OPENSSL_NO_ASM)
# include "./openssl_ocsp_no-asm.h"
#else
# include "./openssl_ocsp_asm.h"
#endif
