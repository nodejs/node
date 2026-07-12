#if defined(OPENSSL_NO_ASM)
# include "./ocsp_no-asm.h"
#else
# include "./ocsp_asm.h"
#endif
