#ifndef SRC_INSPECTOR_IO_AGENT_H_
#define SRC_INSPECTOR_IO_AGENT_H_

#include <unordered_map>
#include "node/inspector/protocol/IO.h"

namespace node {

namespace inspector {
namespace protocol {

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

  void setData(const std::string& key, const std::string value) {
    data_map_[key] = value;
  }

 private:
  std::shared_ptr<IO::Frontend> frontend_;
  std::unordered_map<std::string, int> offset_map_;
  std::unordered_map<std::string, std::string> data_map_;
};
}  // namespace protocol
}  // namespace inspector
}  // namespace node

#endif  // SRC_INSPECTOR_IO_AGENT_H_
