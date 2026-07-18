#include "permission/crypto_store_permission.h"

#include <string>
#include <vector>

namespace node {

namespace permission {

// CryptoStorePermission manages a single global deny state for loading
// private keys through OpenSSL STORE loaders.
void CryptoStorePermission::Apply(Environment* env,
                                  const std::vector<std::string>& allow,
                                  PermissionScope scope) {
  deny_all_ = true;
}

void CryptoStorePermission::Drop(Environment* env,
                                 PermissionScope scope,
                                 const std::string_view& param) {
  deny_all_ = true;
}

bool CryptoStorePermission::is_granted(Environment* env,
                                       PermissionScope perm,
                                       const std::string_view& param) const {
  return perm != PermissionScope::kCryptoStore || !deny_all_;
}

}  // namespace permission
}  // namespace node
