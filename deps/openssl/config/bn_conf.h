#if defined(OPENSSL_NO_ASM)
# include "./bn_conf_no-asm.h"
#else
# include "./bn_conf_asm.h"
#endif
