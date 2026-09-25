#include "io_agent.h"
#include <algorithm>
#include <iostream>
#include <string>
#include <string_view>
#include "crdtp/dispatch.h"
#include "inspector/network_resource_manager.h"

namespace node::inspector::protocol {

void IoAgent::Wire(UberDispatcher* dispatcher) {
  frontend_ = std::make_shared<IO::Frontend>(dispatcher->channel());
  IO::Dispatcher::wire(dispatcher, this);
}

DispatchResponse IoAgent::read(const String& in_handle,
                               std::optional<int> in_offset,
                               std::optional<int> in_size,
                               String* out_data,
                               bool* out_eof) {
  std::string url = in_handle;
  std::string txt = network_resource_manager_->Get(url);
  std::string_view txt_view(txt);

  int offset = 0;
  bool offset_was_specified = false;
  if (in_offset.has_value()) {
    offset = *in_offset;
    offset_was_specified = true;
  } else if (offset_map_.contains(url)) {
    offset = offset_map_[url];
  }
  int size = 1 << 20;
  if (in_size.has_value()) {
    size = *in_size;
  }
  if (static_cast<std::size_t>(offset) < txt_view.length()) {
    std::string_view out_view = txt_view.substr(offset, size);
    out_data->assign(out_view.data(), out_view.size());
    *out_eof = false;
    if (!offset_was_specified) {
      offset_map_[url] = offset + size;
    }
  } else {
    *out_data = "";
    *out_eof = true;
  }

  return DispatchResponse::Success();
}

DispatchResponse IoAgent::close(const String& in_handle) {
  std::string url = in_handle;
  network_resource_manager_->Erase(url);
  return DispatchResponse::Success();
}
}  // namespace node::inspector::protocol
