#if defined(OPENSSL_NO_ASM)
# include "./configuration_no-asm.h"
#else
# include "./configuration_asm.h"
#endif
