#ifndef SRC_INSPECTOR_IO_AGENT_H_
#define SRC_INSPECTOR_IO_AGENT_H_

#include <memory>
#include "inspector/network_resource_manager.h"
#include "node/inspector/protocol/IO.h"

namespace node::inspector::protocol {

class IoAgent : public IO::Backend {
 public:
  explicit IoAgent(
      std::shared_ptr<NetworkResourceManager> network_resource_manager)
      : network_resource_manager_(std::move(network_resource_manager)) {}
  void Wire(UberDispatcher* dispatcher);
  DispatchResponse read(const String& in_handle,
                        std::optional<int> in_offset,
                        std::optional<int> in_size,
                        String* out_data,
                        bool* out_eof) override;
  DispatchResponse close(const String& in_handle) override;

 private:
  std::shared_ptr<IO::Frontend> frontend_;
  std::unordered_map<std::string, int> offset_map_ =
      {};  // Maps stream_id to offset
  std::shared_ptr<NetworkResourceManager> network_resource_manager_;
};
}  // namespace node::inspector::protocol
#endif  // SRC_INSPECTOR_IO_AGENT_H_
