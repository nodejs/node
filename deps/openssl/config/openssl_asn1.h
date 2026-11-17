#if defined(OPENSSL_NO_ASM)
# include "./openssl_asn1_no-asm.h"
#else
# include "./openssl_asn1_asm.h"
#endif
