#if defined(OPENSSL_NO_ASM)
# include "./ct_no-asm.h"
#else
# include "./ct_asm.h"
#endif
