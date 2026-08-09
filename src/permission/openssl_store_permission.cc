#include "permission/openssl_store_permission.h"

#include <string>
#include <vector>

namespace node {

namespace permission {

// OpenSSLStorePermission manages a single global deny state for the use of
// OpenSSL STORE loaders.
void OpenSSLStorePermission::Apply(Environment* env,
                                   std::span<const std::string> allow,
                                   PermissionScope scope) {
  deny_all_ = true;
}

void OpenSSLStorePermission::Drop(Environment* env,
                                  PermissionScope scope,
                                  std::string_view param) {
  deny_all_ = true;
}

bool OpenSSLStorePermission::is_granted(Environment* env,
                                        PermissionScope perm,
                                        std::string_view param) const {
  return perm != PermissionScope::kOpenSSLStore || !deny_all_;
}

}  // namespace permission
}  // namespace node
