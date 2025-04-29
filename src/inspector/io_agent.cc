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
                               Maybe<int> in_offset,
                               Maybe<int> in_size,
                               String* out_data,
                               bool* out_eof) {
  std::string in_handle_str = in_handle;
  uint64_t stream_id = 0;
  bool is_number =
      std::all_of(in_handle_str.begin(), in_handle_str.end(), ::isdigit);
  if (!is_number) {
    *out_data = "";
    *out_eof = true;
    return DispatchResponse::Success();
  }
  stream_id = std::stoull(in_handle_str);

  std::string url = NetworkResourceManager::GetUrlForStreamId(stream_id);
  if (url.empty()) {
    *out_data = "";
    *out_eof = true;
    return DispatchResponse::Success();
  }
  std::string txt = NetworkResourceManager::Get(url);
  std::string_view txt_view(txt);

  int offset = 0;
  bool offset_was_specified = false;
  if (in_offset.isJust()) {
    offset = in_offset.fromJust();
    offset_was_specified = true;
  } else if (offset_map_.find(stream_id) != offset_map_.end()) {
    offset = offset_map_[stream_id];
  }
  int size = 1 << 20;
  if (in_size.isJust()) {
    size = in_size.fromJust();
  }
  if (static_cast<std::size_t>(offset) < txt_view.length()) {
    std::string_view out_view = txt_view.substr(offset, size);
    out_data->assign(out_view.data(), out_view.size());
    *out_eof = false;
    if (!offset_was_specified) {
      offset_map_[stream_id] = offset + size;
    }
  } else {
    *out_data = "";
    *out_eof = true;
  }

  return DispatchResponse::Success();
}

DispatchResponse IoAgent::close(const String& in_handle) {
  std::string in_handle_str = in_handle;
  uint64_t stream_id = 0;
  bool is_number =
      std::all_of(in_handle_str.begin(), in_handle_str.end(), ::isdigit);
  if (is_number) {
    stream_id = std::stoull(in_handle_str);
    // Use accessor to erase resource and mapping by stream id
    NetworkResourceManager::EraseByStreamId(stream_id);
  }
  return DispatchResponse::Success();
}
}  // namespace node::inspector::protocol
