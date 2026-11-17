#if defined(OPENSSL_NO_ASM)
# include "./crypto_dso_conf_no-asm.h"
#else
# include "./crypto_dso_conf_asm.h"
#endif
