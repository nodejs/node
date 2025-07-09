#ifndef SRC_INSPECTOR_IO_AGENT_H_
#define SRC_INSPECTOR_IO_AGENT_H_

#include "node/inspector/protocol/IO.h"

namespace node::inspector::protocol {

class IoAgent : public IO::Backend {
 public:
  IoAgent() {}
  void Wire(UberDispatcher* dispatcher);
  DispatchResponse read(const String& in_handle,
                        Maybe<int> in_offset,
                        Maybe<int> in_size,
                        String* out_data,
                        bool* out_eof) override;
  DispatchResponse close(const String& in_handle) override;

 private:
  std::shared_ptr<IO::Frontend> frontend_;
  std::unordered_map<int, int> offset_map_ = {};  // Maps stream_id to offset
};
}  // namespace node::inspector::protocol
#endif  // SRC_INSPECTOR_IO_AGENT_H_
