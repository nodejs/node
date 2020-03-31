#ifndef SRC_SNAPSHOT_SUPPORT_INL_H_
#define SRC_SNAPSHOT_SUPPORT_INL_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "snapshot_support.h"

namespace node {

template <typename... Args>
ExternalReferences::ExternalReferences(const char* id, Args*... args) {
  Register(id, this);
  HandleArgs(args...);
}

void ExternalReferences::HandleArgs() {}

template <typename T, typename... Args>
void ExternalReferences::HandleArgs(T* ptr, Args*... args) {
  AddPointer(reinterpret_cast<intptr_t>(ptr));
  HandleArgs(args...);
}

}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_SNAPSHOT_SUPPORT_INL_H_
