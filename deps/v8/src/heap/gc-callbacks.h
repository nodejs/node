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
#include "src/common/assert-scope.h"

namespace v8::internal {

class GCCallbacks final {
 public:
  using CallbackType = void (*)(v8::Isolate*, GCType, GCCallbackFlags, void*);

  void Add(CallbackType callback, v8::Isolate* isolate, GCType gc_type,
           void* data) {
    DCHECK_NOT_NULL(callback);
    DCHECK_EQ(callbacks_.end(), FindCallback(callback, data));
    callbacks_.emplace_back(callback, isolate, gc_type, data);
  }

  void Remove(CallbackType callback, void* data) {
    auto it = FindCallback(callback, data);
    DCHECK_NE(callbacks_.end(), it);
    *it = callbacks_.back();
    callbacks_.pop_back();
  }

  void Invoke(GCType gc_type, GCCallbackFlags gc_callback_flags) const {
    AllowGarbageCollection scope;
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
    CallbackData(CallbackType callback, v8::Isolate* isolate, GCType gc_type,
                 void* user_data)
        : callback(callback),
          isolate(isolate),
          gc_type(gc_type),
          user_data(user_data) {}

    CallbackType callback;
    v8::Isolate* isolate;
    GCType gc_type;
    void* user_data;
  };

  std::vector<CallbackData>::iterator FindCallback(CallbackType callback,
                                                   void* data) {
    return std::find_if(callbacks_.begin(), callbacks_.end(),
                        [callback, data](CallbackData& callback_data) {
                          return callback_data.callback == callback &&
                                 callback_data.user_data == data;
                        });
  }

  std::vector<CallbackData> callbacks_;
};

class GCCallbacksInSafepoint final {
 public:
  using CallbackType = void (*)(void*);

  enum GCType { kLocal = 1 << 0, kShared = 1 << 1, kAll = kLocal | kShared };

  void Add(CallbackType callback, void* data, GCType gc_type) {
    DCHECK_NOT_NULL(callback);
    DCHECK_EQ(callbacks_.end(), FindCallback(callback, data));
    callbacks_.emplace_back(callback, data, gc_type);
  }

  void Remove(CallbackType callback, void* data) {
    auto it = FindCallback(callback, data);
    DCHECK_NE(callbacks_.end(), it);
    *it = callbacks_.back();
    callbacks_.pop_back();
  }

  void Invoke(GCType gc_type) const {
    DisallowGarbageCollection scope;
    for (const CallbackData& callback_data : callbacks_) {
      if (callback_data.gc_type_ & gc_type)
        callback_data.callback(callback_data.user_data);
    }
  }

  bool IsEmpty() const { return callbacks_.empty(); }

 private:
  struct CallbackData final {
    CallbackData(CallbackType callback, void* user_data, GCType gc_type)
        : callback(callback), user_data(user_data), gc_type_(gc_type) {}

    CallbackType callback;
    void* user_data;
    GCType gc_type_;
  };

  std::vector<CallbackData>::iterator FindCallback(CallbackType callback,
                                                   void* data) {
    return std::find_if(callbacks_.begin(), callbacks_.end(),
                        [callback, data](CallbackData& callback_data) {
                          return callback_data.callback == callback &&
                                 callback_data.user_data == data;
                        });
  }

  std::vector<CallbackData> callbacks_;
};

}  // namespace v8::internal

#endif  // V8_HEAP_GC_CALLBACKS_H_
