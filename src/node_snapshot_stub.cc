#include "node_main_instance.h"

namespace node {

v8::StartupData* NodeMainInstance::GetEmbeddedSnapshotBlob() {
  return nullptr;
}

const std::vector<size_t>* NodeMainInstance::GetIsolateDataIndexes() {
  return nullptr;
}

}  // namespace node
