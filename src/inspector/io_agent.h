#ifndef SRC_INSPECTOR_IO_AGENT_H_
#define SRC_INSPECTOR_IO_AGENT_H_

#include <unordered_map>
#include "node/inspector/protocol/IO.h"
#include "node_mutex.h"

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

  static int setData(const std::string& value);

 private:
  std::shared_ptr<IO::Frontend> frontend_;
  static int getNextStreamId();

  static std::unordered_map<int, std::string> data_map_;
  static std::unordered_map<int, int> offset_map_;
  static std::atomic<int> stream_counter_;
  static Mutex data_mutex_;
};
}  // namespace node::inspector::protocol
#endif  // SRC_INSPECTOR_IO_AGENT_H_
