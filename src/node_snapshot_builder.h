
#ifndef SRC_NODE_SNAPSHOT_BUILDER_H_
#define SRC_NODE_SNAPSHOT_BUILDER_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include <cstdint>
#include "node_mutex.h"
#include "v8.h"

namespace node {

class ExternalReferenceRegistry;
struct SnapshotData;

class NODE_EXTERN_PRIVATE SnapshotBuilder {
 public:
  static std::string Generate(const std::vector<std::string> args,
                              const std::vector<std::string> exec_args);

  // Generate the snapshot into out.
  static void Generate(SnapshotData* out,
                       const std::vector<std::string> args,
                       const std::vector<std::string> exec_args);

  // If nullptr is returned, the binary is not built with embedded
  // snapshot.
  static const SnapshotData* GetEmbeddedSnapshotData();
  static void InitializeIsolateParams(const SnapshotData* data,
                                      v8::Isolate::CreateParams* params);

 private:
  // Used to synchronize access to the snapshot data
  static Mutex snapshot_data_mutex_;
  static const std::vector<intptr_t>& CollectExternalReferences();

  static std::unique_ptr<ExternalReferenceRegistry> registry_;
};
}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_NODE_SNAPSHOT_BUILDER_H_
