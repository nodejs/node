
#ifndef SRC_NODE_SNAPSHOT_BUILDER_H_
#define SRC_NODE_SNAPSHOT_BUILDER_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include <cstdint>
#include <optional>
#include <string_view>
#include "node_exit_code.h"
#include "node_mutex.h"
#include "v8.h"

namespace node {

class ExternalReferenceRegistry;
struct SnapshotData;

class NODE_EXTERN_PRIVATE SnapshotBuilder {
 public:
  static ExitCode GenerateAsSource(
      const char* out_path,
      const std::vector<std::string>& args,
      const std::vector<std::string>& exec_args,
      std::optional<std::string_view> main_script_path = std::nullopt,
      bool use_array_literals = false);

  // Generate the snapshot into out.
  static ExitCode Generate(SnapshotData* out,
                           const std::vector<std::string>& args,
                           const std::vector<std::string>& exec_args,
                           std::optional<std::string_view> main_script);

  // If nullptr is returned, the binary is not built with embedded
  // snapshot.
  static const SnapshotData* GetEmbeddedSnapshotData();
  static void InitializeIsolateParams(const SnapshotData* data,
                                      v8::Isolate::CreateParams* params);

  static const std::vector<intptr_t>& CollectExternalReferences();

  static ExitCode CreateSnapshot(
      SnapshotData* out,
      CommonEnvironmentSetup* setup,
      /*SnapshotMetadata::Type*/ uint8_t snapshot_type);

 private:
  static std::unique_ptr<ExternalReferenceRegistry> registry_;
};
}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_NODE_SNAPSHOT_BUILDER_H_
