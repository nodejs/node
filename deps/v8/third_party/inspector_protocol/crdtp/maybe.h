// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef V8_CRDTP_MAYBE_H_
#define V8_CRDTP_MAYBE_H_

#include <cassert>
#include <memory>

namespace v8_crdtp {

// =============================================================================
// detail::PtrMaybe, detail::ValueMaybe, templates for optional
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
  T takeJust() {
    assert(is_just_);
    is_just_ = false;
    return std::move(value_);
  }

 private:
  bool is_just_;
  T value_;
};

template <typename T>
struct MaybeTypedef {
  typedef PtrMaybe<T> type;
};

template <>
struct MaybeTypedef<bool> {
  typedef ValueMaybe<bool> type;
};

template <>
struct MaybeTypedef<int> {
  typedef ValueMaybe<int> type;
};

template <>
struct MaybeTypedef<double> {
  typedef ValueMaybe<double> type;
};

template <>
struct MaybeTypedef<std::string> {
  typedef ValueMaybe<std::string> type;
};

}  // namespace detail

template <typename T>
using Maybe = typename detail::MaybeTypedef<T>::type;

}  // namespace v8_crdtp

#endif  // V8_CRDTP_MAYBE_H_
