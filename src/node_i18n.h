#ifndef SRC_NODE_I18N_H_
#define SRC_NODE_I18N_H_

#include "node.h"

#if defined(NODE_HAVE_I18N_SUPPORT)

namespace node {
namespace i18n {

bool InitializeICUDirectory(const char* icu_data_path);

}  // namespace i18n
}  // namespace node

#endif  // NODE_HAVE_I18N_SUPPORT

#endif  // SRC_NODE_I18N_H_
