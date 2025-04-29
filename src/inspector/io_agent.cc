#include "io_agent.h"
#include "crdtp/dispatch.h"

namespace node {
namespace inspector {
namespace protocol {

void IoAgent::Wire(UberDispatcher* dispatcher) {
  frontend_ = std::make_shared<IO::Frontend>(dispatcher->channel());
  IO::Dispatcher::wire(dispatcher, this);
}

DispatchResponse IoAgent::read(const String& in_handle,
                               Maybe<int> in_offset,
                               Maybe<int> in_size,
                               String* out_data,
                               bool* out_eof) {
  std::string txt = data_map_[in_handle];
  int offset = 0;
  if (in_offset.isJust()) {
    offset = in_offset.fromJust();
  } else if (offset_map_.find(in_handle) != offset_map_.end()) {
    offset = offset_map_[in_handle];
  }
  int size = 1 << 20;
  if (in_size.isJust()) {
    size = in_size.fromJust();
  }

  if (static_cast<std::size_t>(offset) < txt.length()) {
    std::string out_txt = txt.substr(offset, size);
    out_data->assign(out_txt);
  } else {
    *out_eof = true;
  }

  offset_map_[in_handle] = offset + size;

  return DispatchResponse::Success();
}

DispatchResponse IoAgent::close(const String& in_handle) {
  offset_map_.erase(in_handle);
  data_map_.erase(in_handle);
  return DispatchResponse::Success();
}

}  // namespace protocol
}  // namespace inspector
}  // namespace node
