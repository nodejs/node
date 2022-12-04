// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_GC_CALLBACKS_H_
#define V8_HEAP_GC_CALLBACKS_H_

#include <algorithm>
#include <tuple>
#include <vector>

#include "include/v8-callbacks.h"
#include "src/base/logging.h"

namespace v8::internal {

template <typename IsolateType, typename InvokeScope>
class GCCallbacks final {
 public:
  using CallbackType = void (*)(IsolateType*, GCType, GCCallbackFlags, void*);

  constexpr GCCallbacks() = default;

  void Add(CallbackType callback, IsolateType* isolate, GCType gc_type,
           void* data) {
    DCHECK_NOT_NULL(callback);
    DCHECK_EQ(callbacks_.end(),
              std::find(callbacks_.begin(), callbacks_.end(),
                        CallbackData{callback, isolate, gc_type, data}));
    callbacks_.emplace_back(callback, isolate, gc_type, data);
  }

  void Remove(CallbackType callback, void* data) {
    auto it =
        std::find(callbacks_.begin(), callbacks_.end(),
                  CallbackData{callback, nullptr, GCType::kGCTypeAll, data});
    DCHECK_NE(callbacks_.end(), it);
    *it = callbacks_.back();
    callbacks_.pop_back();
  }

  void Invoke(GCType gc_type, GCCallbackFlags gc_callback_flags) const {
    InvokeScope scope;
    for (const CallbackData& callback_data : callbacks_) {
      if (gc_type & callback_data.gc_type) {
        callback_data.callback(callback_data.isolate, gc_type,
                               gc_callback_flags, callback_data.user_data);
      }
    }
  }

  bool IsEmpty() const { return callbacks_.empty(); }

 private:
  struct CallbackData final {
    CallbackData(CallbackType callback, IsolateType* isolate, GCType gc_type,
                 void* user_data)
        : callback(callback),
          isolate(isolate),
          gc_type(gc_type),
          user_data(user_data) {}

    bool operator==(const CallbackData& other) const {
      return callback == other.callback && user_data == other.user_data;
    }

    CallbackType callback;
    IsolateType* isolate;
    GCType gc_type;
    void* user_data;
  };

  std::vector<CallbackData> callbacks_;
};

}  // namespace v8::internal

#endif  // V8_HEAP_GC_CALLBACKS_H_
