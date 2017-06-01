#ifndef SRC_NODE_I18N_H_
#define SRC_NODE_I18N_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "node.h"
#include <string>

#if defined(NODE_HAVE_I18N_SUPPORT)

namespace node {

extern bool flag_icu_data_dir;

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
