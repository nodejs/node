#if defined(OPENSSL_NO_ASM)
# include "./core_names_no-asm.h"
#else
# include "./core_names_asm.h"
#endif
