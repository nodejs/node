#ifndef TOOLS_SNAPSHOT_SNAPSHOT_BUILDER_H_
#define TOOLS_SNAPSHOT_SNAPSHOT_BUILDER_H_

#include <string>
#include <vector>

namespace node {
class SnapshotBuilder {
 public:
  static std::string Generate(const std::vector<std::string> args,
                              const std::vector<std::string> exec_args);
};
}  // namespace node

#endif  // TOOLS_SNAPSHOT_SNAPSHOT_BUILDER_H_
