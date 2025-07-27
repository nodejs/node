#pragma once

#if HAVE_OPENSSL
#include <openssl/opensslv.h>
// QUIC is only available in Openssl 3.5.x and later. It was not introduced in
// Node.js until 3.5.1... prior to that we will not compile any of the QUIC
// related code.
#if OPENSSL_VERSION_NUMBER < 0x30500010
#define OPENSSL_NO_QUIC = 1
#endif
#else
#define OPENSSL_NO_QUIC = 1
#endif
