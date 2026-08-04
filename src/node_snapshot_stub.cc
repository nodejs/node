// This file is part of the embedder test, which is intentionally built without
// NODE_WANT_INTERNALS, so we define it here manually.
#define NODE_WANT_INTERNALS 1

#include "node_snapshot_builder.h"

namespace node {

const SnapshotData* SnapshotBuilder::GetEmbeddedSnapshotData() {
  return nullptr;
}

}  // namespace node
