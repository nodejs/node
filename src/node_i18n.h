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

#ifndef SRC_NODE_I18N_H_
#define SRC_NODE_I18N_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "node_internals.h"
#include <string>

#if defined(NODE_HAVE_I18N_SUPPORT)

namespace node {

namespace i18n {

bool InitializeICUDirectory(const std::string& path);

enum idna_mode {
  // Default mode for maximum compatibility.
  IDNA_DEFAULT,
  // Ignore all errors in IDNA conversion, if possible.
  IDNA_LENIENT,
  // Enforce STD3 rules (UseSTD3ASCIIRules) and DNS length restrictions
  // (VerifyDnsLength). Corresponds to `beStrict` flag in the "domain to ASCII"
  // algorithm.
  IDNA_STRICT
};

// Implements the WHATWG URL Standard "domain to ASCII" algorithm.
// https://url.spec.whatwg.org/#concept-domain-to-ascii
int32_t ToASCII(MaybeStackBuffer<char>* buf,
                const char* input,
                size_t length,
                enum idna_mode mode = IDNA_DEFAULT);

// Implements the WHATWG URL Standard "domain to Unicode" algorithm.
// https://url.spec.whatwg.org/#concept-domain-to-unicode
int32_t ToUnicode(MaybeStackBuffer<char>* buf,
                  const char* input,
                  size_t length);

}  // namespace i18n
}  // namespace node

#endif  // NODE_HAVE_I18N_SUPPORT

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_NODE_I18N_H_
