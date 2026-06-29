#if defined(OPENSSL_NO_ASM)
# include "./safestack_no-asm.h"
#else
# include "./safestack_asm.h"
#endif
