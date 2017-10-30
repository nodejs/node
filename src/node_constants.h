// Copyright Joyent, Inc. and other Node contributors.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to permit
// persons to whom the Software is furnished to do so, subject to the
// following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
// USE OR OTHER DEALINGS IN THE SOFTWARE.

#ifndef SRC_NODE_CONSTANTS_H_
#define SRC_NODE_CONSTANTS_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "node.h"
#include "v8.h"

#if HAVE_OPENSSL

#ifndef RSA_PSS_SALTLEN_DIGEST
#define RSA_PSS_SALTLEN_DIGEST -1
#endif

#ifndef RSA_PSS_SALTLEN_MAX_SIGN
#define RSA_PSS_SALTLEN_MAX_SIGN -2
#endif

#ifndef RSA_PSS_SALTLEN_AUTO
#define RSA_PSS_SALTLEN_AUTO -2
#endif

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
