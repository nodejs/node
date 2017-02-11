#ifndef SRC_NODE_I18N_H_
#define SRC_NODE_I18N_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "node.h"
#include <string>

#if defined(NODE_HAVE_I18N_SUPPORT)

namespace node {

extern std::string icu_data_dir;  // NOLINT(runtime/string)

namespace i18n {

bool InitializeICUDirectory(const std::string& path);

int32_t ToASCII(MaybeStackBuffer<char>* buf,
                const char* input,
                size_t length);
int32_t ToUnicode(MaybeStackBuffer<char>* buf,
                  const char* input,
                  size_t length);

}  // namespace i18n
}  // namespace node

#endif  // NODE_HAVE_I18N_SUPPORT

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_NODE_I18N_H_
