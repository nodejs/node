// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_EXECUTION_LOCAL_ISOLATE_WRAPPER_INL_H_
#define V8_EXECUTION_LOCAL_ISOLATE_WRAPPER_INL_H_

#include "src/execution/isolate.h"
#include "src/execution/local-isolate-wrapper.h"
#include "src/execution/off-thread-isolate.h"
#include "src/heap/heap-inl.h"
#include "src/heap/off-thread-heap.h"
#include "src/logging/log.h"
#include "src/logging/off-thread-logger.h"

namespace v8 {
namespace internal {

class HeapMethodCaller {
 public:
  explicit HeapMethodCaller(LocalHeapWrapper* heap) : heap_(heap) {}

  ReadOnlySpace* read_only_space() {
    return heap_->is_off_thread() ? heap_->off_thread()->read_only_space()
                                  : heap_->main_thread()->read_only_space();
  }

  void OnAllocationEvent(HeapObject obj, int size) {
    return heap_->is_off_thread()
               ? heap_->off_thread()->OnAllocationEvent(obj, size)
               : heap_->main_thread()->OnAllocationEvent(obj, size);
  }

  bool Contains(HeapObject obj) {
    return heap_->is_off_thread() ? heap_->off_thread()->Contains(obj)
                                  : heap_->main_thread()->Contains(obj);
  }

 private:
  LocalHeapWrapper* heap_;
};

class LoggerMethodCaller {
 public:
  explicit LoggerMethodCaller(LocalLoggerWrapper* logger) : logger_(logger) {}

  bool is_logging() const {
    return logger_->is_off_thread() ? logger_->off_thread()->is_logging()
                                    : logger_->main_thread()->is_logging();
  }

  void ScriptEvent(Logger::ScriptEventType type, int script_id) {
    return logger_->is_off_thread()
               ? logger_->off_thread()->ScriptEvent(type, script_id)
               : logger_->main_thread()->ScriptEvent(type, script_id);
  }
  void ScriptDetails(Script script) {
    return logger_->is_off_thread()
               ? logger_->off_thread()->ScriptDetails(script)
               : logger_->main_thread()->ScriptDetails(script);
  }

 private:
  LocalLoggerWrapper* logger_;
};

class IsolateMethodCaller {
 public:
  explicit IsolateMethodCaller(LocalIsolateWrapper* isolate)
      : isolate_(isolate) {}

  LocalLoggerWrapper logger() {
    return isolate_->is_off_thread()
               ? LocalLoggerWrapper(isolate_->off_thread()->logger())
               : LocalLoggerWrapper(isolate_->main_thread()->logger());
  }

  LocalHeapWrapper heap() {
    return isolate_->is_off_thread()
               ? LocalHeapWrapper(isolate_->off_thread()->heap())
               : LocalHeapWrapper(isolate_->main_thread()->heap());
  }

  ReadOnlyHeap* read_only_heap() {
    return isolate_->is_off_thread()
               ? isolate_->off_thread()->read_only_heap()
               : isolate_->main_thread()->read_only_heap();
  }

  Object root(RootIndex index) {
    return isolate_->is_off_thread() ? isolate_->off_thread()->root(index)
                                     : isolate_->main_thread()->root(index);
  }

  int GetNextScriptId() {
    return isolate_->is_off_thread()
               ? isolate_->off_thread()->GetNextScriptId()
               : isolate_->main_thread()->GetNextScriptId();
  }

 private:
  LocalIsolateWrapper* isolate_;
};

// Helper wrapper for HandleScope behaviour with a LocalIsolateWrapper.
class LocalHandleScopeWrapper {
 public:
  explicit LocalHandleScopeWrapper(LocalIsolateWrapper local_isolate)
      : is_off_thread_(local_isolate.is_off_thread()) {
    if (is_off_thread_) {
      new (off_thread()) OffThreadHandleScope(local_isolate.off_thread());
    } else {
      new (main_thread()) HandleScope(local_isolate.main_thread());
    }
  }
  ~LocalHandleScopeWrapper() {
    if (is_off_thread_) {
      off_thread()->~OffThreadHandleScope();
    } else {
      main_thread()->~HandleScope();
    }
  }

  template <typename T>
  Handle<T> CloseAndEscape(Handle<T> handle) {
    if (is_off_thread_) {
      return off_thread()->CloseAndEscape(handle);
    } else {
      return main_thread()->CloseAndEscape(handle);
    }
  }

 private:
  HandleScope* main_thread() {
    return reinterpret_cast<HandleScope*>(&scope_storage_);
  }
  OffThreadHandleScope* off_thread() {
    return reinterpret_cast<OffThreadHandleScope*>(&scope_storage_);
  }

  std::aligned_union_t<0, HandleScope, OffThreadHandleScope> scope_storage_;
  bool is_off_thread_;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_EXECUTION_LOCAL_ISOLATE_WRAPPER_INL_H_
