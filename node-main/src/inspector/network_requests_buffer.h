#ifndef SRC_INSPECTOR_NETWORK_REQUESTS_BUFFER_H_
#define SRC_INSPECTOR_NETWORK_REQUESTS_BUFFER_H_

#include <list>
#include <map>
#include <vector>

#include "inspector/node_string.h"

namespace node {
namespace inspector {

// Supported charsets for devtools frontend on request/response data.
// If the charset is kUTF8, the data is expected to be text and can be
// formatted on the frontend.
enum class Charset {
  kUTF8,
  kBinary,
};

struct RequestEntry {
  double timestamp;
  bool is_request_finished = false;
  bool is_response_finished = false;
  bool is_streaming = false;
  Charset request_charset;
  Charset response_charset = Charset::kBinary;

  size_t max_resource_buffer_size;
  size_t buffer_size() const {
    return request_data_size_ + response_data_size_;
  }

  const std::vector<protocol::Binary> request_data_blobs() const {
    return request_data_blobs_;
  }
  const std::vector<protocol::Binary> response_data_blobs() const {
    return response_data_blobs_;
  }

  void push_request_data_blob(const protocol::Binary& blob) {
    if (buffer_size() + blob.size() > max_resource_buffer_size) {
      // Exceed the per-resource buffer size limit, ignore the blob.
      return;
    }
    request_data_blobs_.push_back(blob);
    request_data_size_ += blob.size();
  }
  void push_response_data_blob(const protocol::Binary& blob) {
    if (buffer_size() + blob.size() > max_resource_buffer_size) {
      // Exceed the per-resource buffer size limit, ignore the blob.
      return;
    }
    response_data_blobs_.push_back(blob);
    response_data_size_ += blob.size();
  }

  void clear_response_data_blobs() {
    response_data_blobs_.clear();
    response_data_size_ = 0;
  }

  RequestEntry(double timestamp,
               Charset request_charset,
               bool has_request_body,
               size_t max_resource_buffer_size)
      : timestamp(timestamp),
        is_request_finished(!has_request_body),
        request_charset(request_charset),
        max_resource_buffer_size(max_resource_buffer_size) {}

 private:
  size_t request_data_size_ = 0;
  std::vector<protocol::Binary> request_data_blobs_;
  size_t response_data_size_ = 0;
  std::vector<protocol::Binary> response_data_blobs_;
};

class RequestsBuffer {
  using key_value_pair_t = typename std::pair<protocol::String, RequestEntry>;
  using list_iterator = typename std::list<key_value_pair_t>::iterator;
  using const_list_iterator =
      typename std::list<key_value_pair_t>::const_iterator;

 public:
  struct mutable_iterator {
    list_iterator it;
    size_t buffer_size;
    RequestsBuffer* parent;

    key_value_pair_t& operator*() { return *it; }
    key_value_pair_t* operator->() { return &(*it); }
    bool operator==(const const_list_iterator& other) const {
      return it == other;
    }
    bool operator!=(const const_list_iterator& other) const {
      return it != other;
    }

    mutable_iterator(std::nullptr_t, RequestsBuffer* parent)
        : it(parent->list_.end()), buffer_size(0), parent(parent) {}

    mutable_iterator(list_iterator it, RequestsBuffer* parent)
        : it(it), buffer_size(it->second.buffer_size()), parent(parent) {}
    ~mutable_iterator() {
      if (it == parent->list_.end()) return;
      // The entry must not have been erased.
      DCHECK(parent->lookup_map_.contains(it->first));
      // Update the total buffer size if the buffer size has changed.
      if (buffer_size == it->second.buffer_size()) {
        return;
      }
      parent->total_buffer_size_ += it->second.buffer_size();
      DCHECK_GE(parent->total_buffer_size_, buffer_size);
      parent->total_buffer_size_ -= buffer_size;
      parent->enforceBufferLimits();
    }
  };

  explicit RequestsBuffer(size_t max_total_buffer_size)
      : max_total_buffer_size_(max_total_buffer_size) {}

  size_t max_total_buffer_size() const { return max_total_buffer_size_; }
  size_t total_buffer_size() const { return total_buffer_size_; }

  const_list_iterator begin() const { return list_.begin(); }
  const_list_iterator end() const { return list_.end(); }

  // Get a mutable iterator to the entry with the given key.
  // The iterator will update the total buffer size when it is destroyed.
  mutable_iterator find(const protocol::String& key) {
    auto it = lookup_map_.find(key);
    if (it == lookup_map_.end()) {
      return mutable_iterator(nullptr, this);
    }
    return mutable_iterator(it->second, this);
  }
  const_list_iterator cfind(const protocol::String& key) const {
    auto it = lookup_map_.find(key);
    if (it == lookup_map_.end()) {
      return list_.end();
    }
    return it->second;
  }

  bool contains(const protocol::String& key) const {
    return lookup_map_.contains(key);
  }

  bool emplace(const protocol::String& key, RequestEntry&& value) {
    if (lookup_map_.contains(key)) {
      return false;
    }
    list_.emplace_front(key, std::move(value));
    lookup_map_[key] = list_.begin();
    // No buffer yet.
    DCHECK_EQ(begin()->second.buffer_size(), 0u);
    return true;
  }

  void erase(const protocol::String& key) {
    auto it = lookup_map_.find(key);
    if (it == lookup_map_.end()) {
      return;
    }
    erase(it->second);
  }

  void erase(const_list_iterator it) {
    DCHECK(it != list_.end());
    DCHECK_GE(total_buffer_size_, it->second.buffer_size());
    total_buffer_size_ -= it->second.buffer_size();
    lookup_map_.erase(it->first);
    list_.erase(it);
  }

 private:
  void enforceBufferLimits() {
    while (total_buffer_size_ > max_total_buffer_size_ && !list_.empty()) {
      auto last_it = --list_.end();
      erase(last_it);
    }
  }

  size_t max_total_buffer_size_;
  size_t total_buffer_size_ = 0;

  std::list<key_value_pair_t> list_;
  std::map<protocol::String, list_iterator> lookup_map_;
};

}  // namespace inspector
}  // namespace node

#endif  // SRC_INSPECTOR_NETWORK_REQUESTS_BUFFER_H_
