// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_INSPECTOR_PROTOCOL_BINDINGS_BINDINGS_H_
#define V8_INSPECTOR_PROTOCOL_BINDINGS_BINDINGS_H_

#include <cassert>
#include <memory>

namespace v8_inspector_protocol_bindings {
namespace glue {
// =============================================================================
// glue::detail::PtrMaybe, glue::detail::ValueMaybe, templates for optional
// pointers / values which are used in ../lib/Forward_h.template.
// =============================================================================
namespace detail {
template <typename T>
class PtrMaybe {
 public:
  PtrMaybe() = default;
  PtrMaybe(std::unique_ptr<T> value) : value_(std::move(value)) {}
  PtrMaybe(PtrMaybe&& other) noexcept : value_(std::move(other.value_)) {}
  void operator=(std::unique_ptr<T> value) { value_ = std::move(value); }
  T* fromJust() const {
    assert(value_);
    return value_.get();
  }
  T* fromMaybe(T* default_value) const {
    return value_ ? value_.get() : default_value;
  }
  bool isJust() const { return value_ != nullptr; }
  std::unique_ptr<T> takeJust() {
    assert(value_);
    return std::move(value_);
  }

 private:
  std::unique_ptr<T> value_;
};

template <typename T>
class ValueMaybe {
 public:
  ValueMaybe() : is_just_(false), value_() {}
  ValueMaybe(T value) : is_just_(true), value_(std::move(value)) {}
  ValueMaybe(ValueMaybe&& other) noexcept
      : is_just_(other.is_just_), value_(std::move(other.value_)) {}
  void operator=(T value) {
    value_ = value;
    is_just_ = true;
  }
  const T& fromJust() const {
    assert(is_just_);
    return value_;
  }
  const T& fromMaybe(const T& default_value) const {
    return is_just_ ? value_ : default_value;
  }
  bool isJust() const { return is_just_; }
  // TODO(johannes): |is_just_| isn't reset by this operation -
  // introduce && to ensure avoiding continued usage of |this|?
  T takeJust() {
    assert(is_just_);
    return std::move(value_);
  }

 private:
  bool is_just_;
  T value_;
};
}  // namespace detail
}  // namespace glue
}  // namespace v8_inspector_protocol_bindings

#define PROTOCOL_DISALLOW_COPY(ClassName) \
    private: \
        ClassName(const ClassName&) = delete; \
        ClassName& operator=(const ClassName&) = delete

#endif  // V8_INSPECTOR_PROTOCOL_BINDINGS_BINDINGS_H_
