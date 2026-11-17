#if defined(OPENSSL_NO_ASM)
# include "./openssl_ct_no-asm.h"
#else
# include "./openssl_ct_asm.h"
#endif
