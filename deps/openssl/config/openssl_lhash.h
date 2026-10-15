#if defined(OPENSSL_NO_ASM)
# include "./openssl_lhash_no-asm.h"
#else
# include "./openssl_lhash_asm.h"
#endif
