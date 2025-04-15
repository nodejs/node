// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef INCLUDE_V8_WEAK_CALLBACK_INFO_H_
#define INCLUDE_V8_WEAK_CALLBACK_INFO_H_

#include <cstring>

#include "cppgc/internal/conditional-stack-allocated.h"  // NOLINT(build/include_directory)
#include "v8config.h"  // NOLINT(build/include_directory)

namespace v8 {

class Isolate;

namespace api_internal {
V8_EXPORT void InternalFieldOutOfBounds(int index);
}  // namespace api_internal

static constexpr int kInternalFieldsInWeakCallback = 2;
static constexpr int kEmbedderFieldsInWeakCallback = 2;

template <typename T>
class WeakCallbackInfo
    : public cppgc::internal::ConditionalStackAllocatedBase<T> {
 public:
  using Callback = void (*)(const WeakCallbackInfo<T>& data);

  WeakCallbackInfo(Isolate* isolate, T* parameter,
                   void* embedder_fields[kEmbedderFieldsInWeakCallback],
                   Callback* callback)
      : isolate_(isolate), parameter_(parameter), callback_(callback) {
    memcpy(embedder_fields_, embedder_fields,
           sizeof(embedder_fields[0]) * kEmbedderFieldsInWeakCallback);
  }

  V8_INLINE Isolate* GetIsolate() const { return isolate_; }
  V8_INLINE T* GetParameter() const { return parameter_; }
  V8_INLINE void* GetInternalField(int index) const;

  /**
   * When a weak callback is first invoked the embedders _must_ Reset() the
   * handle which triggered the callback. The handle itself is unusable for
   * anything else. No other V8 API calls may be called in the first callback.
   * Additional work requires scheduling a second invocation via
   * `SetSecondPassCallback()` which will be called some time after all the
   * initial callbacks are processed.
   *
   * The second pass callback is prohibited from executing JavaScript. Embedders
   * should schedule another callback in case this is required.
   */
  void SetSecondPassCallback(Callback callback) const { *callback_ = callback; }

 private:
  Isolate* isolate_;
  T* parameter_;
  Callback* callback_;
  void* embedder_fields_[kEmbedderFieldsInWeakCallback];
};

/**
 * Weakness type for weak handles.
 */
enum class WeakCallbackType {
  /**
   * Passes a user-defined void* parameter back to the callback.
   */
  kParameter,
  /**
   * Passes the first two internal fields of the object back to the callback.
   */
  kInternalFields,
};

template <class T>
void* WeakCallbackInfo<T>::GetInternalField(int index) const {
#ifdef V8_ENABLE_CHECKS
  if (index < 0 || index >= kEmbedderFieldsInWeakCallback) {
    api_internal::InternalFieldOutOfBounds(index);
  }
#endif
  return embedder_fields_[index];
}

}  // namespace v8

#endif  // INCLUDE_V8_WEAK_CALLBACK_INFO_H_
