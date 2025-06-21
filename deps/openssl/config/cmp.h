#if defined(OPENSSL_NO_ASM)
# include "./cmp_no-asm.h"
#else
# include "./cmp_asm.h"
#endif
