// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_NODE_AUX_DATA_H_
#define V8_COMPILER_NODE_AUX_DATA_H_

#include "src/compiler/node.h"
#include "src/zone/zone-containers.h"

namespace v8 {
namespace internal {
namespace compiler {

// Forward declarations.
class Node;

template <class T>
T DefaultConstruct() {
  return T();
}

template <class T, T def() = DefaultConstruct<T>>
class NodeAuxData {
 public:
  explicit NodeAuxData(Zone* zone) : aux_data_(zone) {}
  explicit NodeAuxData(size_t initial_size, Zone* zone)
      : aux_data_(initial_size, zone) {}

  // Update entry. Returns true iff entry was changed.
  bool Set(Node* node, T const& data) {
    size_t const id = node->id();
    if (id >= aux_data_.size()) aux_data_.resize(id + 1, def());
    if (aux_data_[id] != data) {
      aux_data_[id] = data;
      return true;
    }
    return false;
  }

  T Get(Node* node) const {
    size_t const id = node->id();
    return (id < aux_data_.size()) ? aux_data_[id] : def();
  }

  class const_iterator;
  friend class const_iterator;

  const_iterator begin() const;
  const_iterator end() const;

 private:
  ZoneVector<T> aux_data_;
};

template <class T, T def()>
class NodeAuxData<T, def>::const_iterator {
 public:
  using iterator_category = std::forward_iterator_tag;
  using difference_type = int;
  using value_type = std::pair<size_t, T>;
  using pointer = value_type*;
  using reference = value_type&;

  const_iterator(const ZoneVector<T>* data, size_t current)
      : data_(data), current_(current) {}
  const_iterator(const const_iterator& other)
      : data_(other.data_), current_(other.current_) {}

  value_type operator*() const {
    return std::make_pair(current_, (*data_)[current_]);
  }
  bool operator==(const const_iterator& other) const {
    return current_ == other.current_ && data_ == other.data_;
  }
  bool operator!=(const const_iterator& other) const {
    return !(*this == other);
  }
  const_iterator& operator++() {
    ++current_;
    return *this;
  }
  const_iterator operator++(int);

 private:
  const ZoneVector<T>* data_;
  size_t current_;
};

template <class T, T def()>
typename NodeAuxData<T, def>::const_iterator NodeAuxData<T, def>::begin()
    const {
  return typename NodeAuxData<T, def>::const_iterator(&aux_data_, 0);
}

template <class T, T def()>
typename NodeAuxData<T, def>::const_iterator NodeAuxData<T, def>::end() const {
  return typename NodeAuxData<T, def>::const_iterator(&aux_data_,
                                                      aux_data_.size());
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_NODE_AUX_DATA_H_
