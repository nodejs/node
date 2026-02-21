#if defined(OPENSSL_NO_ASM)
# include "./pkcs7_no-asm.h"
#else
# include "./pkcs7_asm.h"
#endif
