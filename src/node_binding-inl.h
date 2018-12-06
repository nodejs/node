#ifndef SRC_NODE_BINDING_INL_H_
#define SRC_NODE_BINDING_INL_H_

#include "node_binding.h"

namespace node {

namespace binding {

inline DLib::DLib(const char* filename, int flags):
    filename_(filename), flags_(flags), handle_(nullptr) {}

#ifdef __POSIX__
  inline bool DLib::Open() {
    handle_ = dlopen(filename_.c_str(), flags_);
    if (handle_ != nullptr) return true;
    errmsg_ = dlerror();
    return false;
  }

  inline void DLib::Close() {
    if (handle_ == nullptr) return;
    dlclose(handle_);
    handle_ = nullptr;
  }

  inline void* DLib::GetSymbolAddress(const char* name) {
    return dlsym(handle_, name);
  }
#else   // !__POSIX__
  inline bool DLib::Open() {
    int ret = uv_dlopen(filename_.c_str(), &lib_);
    if (ret == 0) {
      handle_ = static_cast<void*>(lib_.handle);
      return true;
    }
    errmsg_ = uv_dlerror(&lib_);
    uv_dlclose(&lib_);
    return false;
  }

  inline void DLib::Close() {
    if (handle_ == nullptr) return;
    uv_dlclose(&lib_);
    handle_ = nullptr;
  }

  inline void* DLib::GetSymbolAddress(const char* name) {
    void* address;
    if (0 == uv_dlsym(&lib_, name, &address)) return address;
    return nullptr;
  }
#endif  // !__POSIX__

}  // end of namespace binding

}  // end of namespace node

#endif  // SRC_NODE_BINDING_INL_H_
