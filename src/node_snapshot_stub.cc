// This file is part of the embedder test, which is intentionally built without
// NODE_WANT_INTERNALS, so we define it here manually.
#define NODE_WANT_INTERNALS 1

#include "node_main_instance.h"

namespace node {

const SnapshotData* NodeMainInstance::GetEmbeddedSnapshotData() {
  return nullptr;
}

}  // namespace node
