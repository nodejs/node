#include "io_agent.h"
#include <string>
#include "crdtp/dispatch.h"
#include "node_mutex.h"

namespace node::inspector::protocol {

void IoAgent::Wire(UberDispatcher* dispatcher) {
  frontend_ = std::make_shared<IO::Frontend>(dispatcher->channel());
  IO::Dispatcher::wire(dispatcher, this);
}

std::unordered_map<int, int> IoAgent::offset_map_;
std::unordered_map<int, std::string> IoAgent::data_map_;
std::atomic<int> IoAgent::stream_counter_{1};
Mutex IoAgent::data_mutex_;

int IoAgent::setData(const std::string& value) {
  int key = getNextStreamId();
  Mutex::ScopedLock lock(data_mutex_);
  data_map_[key] = value;

  return key;
}

int IoAgent::getNextStreamId() {
  return stream_counter_++;
}

DispatchResponse IoAgent::read(const String& in_handle,
                               Maybe<int> in_offset,
                               Maybe<int> in_size,
                               String* out_data,
                               bool* out_eof) {
  Mutex::ScopedReadLock lock(data_mutex_);
  std::string in_handle_str = in_handle;
  int stream_id = 0;
  bool is_number =
      std::all_of(in_handle_str.begin(), in_handle_str.end(), ::isdigit);
  if (!is_number) {
    out_data = new String("");
    *out_eof = true;
    return DispatchResponse::Success();
  }
  stream_id = std::stoi(in_handle_str);

  std::string txt = data_map_[stream_id];
  int offset = 0;
  if (in_offset.isJust()) {
    offset = in_offset.fromJust();
  } else if (offset_map_.find(stream_id) != offset_map_.end()) {
    offset = offset_map_[stream_id];
  }
  int size = 1 << 20;
  if (in_size.isJust()) {
    size = in_size.fromJust();
  }

  if (static_cast<std::size_t>(offset) < txt.length()) {
    std::string out_txt = txt.substr(offset, size);
    out_data->assign(out_txt);
    *out_eof = false;
  } else {
    *out_eof = true;
  }

  offset_map_[stream_id] = offset + size;

  return DispatchResponse::Success();
}

DispatchResponse IoAgent::close(const String& in_handle) {
  Mutex::ScopedWriteLock lock(data_mutex_);
  std::string in_handle_str = in_handle;
  int stream_id = 0;
  bool is_number =
      std::all_of(in_handle_str.begin(), in_handle_str.end(), ::isdigit);
  if (is_number) {
    stream_id = std::stoi(in_handle_str);
    offset_map_.erase(stream_id);
    data_map_.erase(stream_id);
  }
  return DispatchResponse::Success();
}
}  // namespace node::inspector::protocol
