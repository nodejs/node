#ifndef SRC_NODE_ABI_VERSIONS_H_
#define SRC_NODE_ABI_VERSIONS_H_

typedef enum {
  node_abi_version_terminator,
  node_abi_vendor_version,
  node_abi_engine_version,
  node_abi_openssl_version,
  node_abi_libuv_version,
  node_abi_icu_version,
  node_abi_cares_version
} node_abi_version_item;

typedef struct {
  node_abi_version_item item;
  int version;
} node_abi_version_entry;

#define NODE_ABI_VENDOR_VERSION 1
#define NODE_ABI_ENGINE_VERSION 1
#define NODE_ABI_OPENSSL_VERSION 1
#define NODE_ABI_LIBUV_VERSION 1
#define NODE_ABI_ICU_VERSION 1
#define NODE_ABI_CARES_VERSION 1

#endif  // SRC_NODE_ABI_VERSIONS_H_
