#if defined(OPENSSL_NO_ASM)
# include "./fipskey_no-asm.h"
#else
# include "./fipskey_asm.h"
#endif
