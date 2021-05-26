// This file is part of the embedder test, which is intentionally built without
// NODE_WANT_INTERNALS, so we define it here manually.
#define NODE_WANT_INTERNALS 1

#include "node_main_instance.h"

namespace node {

v8::StartupData* NodeMainInstance::GetEmbeddedSnapshotBlob() {
  return nullptr;
}

const std::vector<size_t>* NodeMainInstance::GetIsolateDataIndices() {
  return nullptr;
}

const EnvSerializeInfo* NodeMainInstance::GetEnvSerializeInfo() {
  return nullptr;
}

}  // namespace node
