#if defined(OPENSSL_NO_ASM)
# include "./lhash_no-asm.h"
#else
# include "./lhash_asm.h"
#endif
