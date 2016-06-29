#ifndef SRC_NODE_I18N_H_
#define SRC_NODE_I18N_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "node.h"

#if defined(NODE_HAVE_I18N_SUPPORT)

namespace node {

extern bool flag_icu_data_dir;

namespace i18n {

bool InitializeICUDirectory(const char* icu_data_path);

}  // namespace i18n
}  // namespace node

#endif  // NODE_HAVE_I18N_SUPPORT

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_NODE_I18N_H_
