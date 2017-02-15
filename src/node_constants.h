#ifndef SRC_NODE_CONSTANTS_H_
#define SRC_NODE_CONSTANTS_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "node.h"
#include "v8.h"

#if HAVE_OPENSSL
#define DEFAULT_CIPHER_LIST_CORE "ECDHE-RSA-AES128-GCM-SHA256:"     \
                                 "ECDHE-ECDSA-AES128-GCM-SHA256:"   \
                                 "ECDHE-RSA-AES256-GCM-SHA384:"     \
                                 "ECDHE-ECDSA-AES256-GCM-SHA384:"   \
                                 "DHE-RSA-AES128-GCM-SHA256:"       \
                                 "ECDHE-RSA-AES128-SHA256:"         \
                                 "DHE-RSA-AES128-SHA256:"           \
                                 "ECDHE-RSA-AES256-SHA384:"         \
                                 "DHE-RSA-AES256-SHA384:"           \
                                 "ECDHE-RSA-AES256-SHA256:"         \
                                 "DHE-RSA-AES256-SHA256:"           \
                                 "HIGH:"                            \
                                 "!aNULL:"                          \
                                 "!eNULL:"                          \
                                 "!EXPORT:"                         \
                                 "!DES:"                            \
                                 "!RC4:"                            \
                                 "!MD5:"                            \
                                 "!PSK:"                            \
                                 "!SRP:"                            \
                                 "!CAMELLIA"
#endif

namespace node {

#if HAVE_OPENSSL
extern const char* default_cipher_list;
#endif

void DefineConstants(v8::Isolate* isolate, v8::Local<v8::Object> target);
}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_NODE_CONSTANTS_H_
