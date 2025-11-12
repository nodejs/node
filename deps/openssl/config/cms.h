#if defined(OPENSSL_NO_ASM)
# include "./cms_no-asm.h"
#else
# include "./cms_asm.h"
#endif
