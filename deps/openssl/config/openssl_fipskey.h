#if defined(OPENSSL_NO_ASM)
# include "./openssl_fipskey_no-asm.h"
#else
# include "./openssl_fipskey_asm.h"
#endif
