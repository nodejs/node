#if defined(OPENSSL_NO_ASM)
# include "./openssl_comp_no-asm.h"
#else
# include "./openssl_comp_asm.h"
#endif
