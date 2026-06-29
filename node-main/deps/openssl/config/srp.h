#if defined(OPENSSL_NO_ASM)
# include "./srp_no-asm.h"
#else
# include "./srp_asm.h"
#endif
