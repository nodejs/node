// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_LOGGING_CODE_EVENTS_H_
#define V8_LOGGING_CODE_EVENTS_H_

#include <vector>

#include "src/base/platform/mutex.h"
#include "src/base/vector.h"
#include "src/common/globals.h"
#include "src/objects/code.h"
#include "src/objects/name.h"
#include "src/objects/shared-function-info.h"
#include "src/objects/string.h"

namespace v8 {
namespace internal {

class AbstractCode;
class Name;
class SharedFunctionInfo;
class String;

namespace wasm {
class WasmCode;
using WasmName = base::Vector<const char>;
}  // namespace wasm

// clang-format off
#define LOG_EVENTS_LIST(V)                             \
  V(CODE_CREATION_EVENT, code-creation)                \
  V(CODE_DISABLE_OPT_EVENT, code-disable-optimization) \
  V(CODE_MOVE_EVENT, code-move)                        \
  V(CODE_DELETE_EVENT, code-delete)                    \
  V(CODE_MOVING_GC, code-moving-gc)                    \
  V(SHARED_FUNC_MOVE_EVENT, sfi-move)                  \
  V(SNAPSHOT_CODE_NAME_EVENT, snapshot-code-name)      \
  V(TICK_EVENT, tick)
// clang-format on

#define TAGS_LIST(V)                       \
  V(BUILTIN_TAG, Builtin)                  \
  V(CALLBACK_TAG, Callback)                \
  V(EVAL_TAG, Eval)                        \
  V(FUNCTION_TAG, Function)                \
  V(HANDLER_TAG, Handler)                  \
  V(BYTECODE_HANDLER_TAG, BytecodeHandler) \
  V(LAZY_COMPILE_TAG, LazyCompile)         \
  V(REG_EXP_TAG, RegExp)                   \
  V(SCRIPT_TAG, Script)                    \
  V(STUB_TAG, Stub)                        \
  V(NATIVE_FUNCTION_TAG, Function)         \
  V(NATIVE_LAZY_COMPILE_TAG, LazyCompile)  \
  V(NATIVE_SCRIPT_TAG, Script)
// Note that 'NATIVE_' cases for functions and scripts are mapped onto
// original tags when writing to the log.

#define LOG_EVENTS_AND_TAGS_LIST(V) \
  LOG_EVENTS_LIST(V)                \
  TAGS_LIST(V)

#define PROFILE(the_isolate, Call) (the_isolate)->log_event_dispatcher()->Call;

class LogEventListener {
 public:
#define DECLARE_ENUM(enum_item, _) enum_item,
  enum LogEventsAndTags {
    LOG_EVENTS_AND_TAGS_LIST(DECLARE_ENUM) NUMBER_OF_LOG_EVENTS
  };
#undef DECLARE_ENUM

  virtual ~LogEventListener() = default;

  virtual void CodeCreateEvent(LogEventsAndTags tag, Handle<AbstractCode> code,
                               const char* name) = 0;
  virtual void CodeCreateEvent(LogEventsAndTags tag, Handle<AbstractCode> code,
                               Handle<Name> name) = 0;
  virtual void CodeCreateEvent(LogEventsAndTags tag, Handle<AbstractCode> code,
                               Handle<SharedFunctionInfo> shared,
                               Handle<Name> script_name) = 0;
  virtual void CodeCreateEvent(LogEventsAndTags tag, Handle<AbstractCode> code,
                               Handle<SharedFunctionInfo> shared,
                               Handle<Name> script_name, int line,
                               int column) = 0;
#if V8_ENABLE_WEBASSEMBLY
  virtual void CodeCreateEvent(LogEventsAndTags tag, const wasm::WasmCode* code,
                               wasm::WasmName name, const char* source_url,
                               int code_offset, int script_id) = 0;
#endif  // V8_ENABLE_WEBASSEMBLY

  virtual void CallbackEvent(Handle<Name> name, Address entry_point) = 0;
  virtual void GetterCallbackEvent(Handle<Name> name, Address entry_point) = 0;
  virtual void SetterCallbackEvent(Handle<Name> name, Address entry_point) = 0;
  virtual void RegExpCodeCreateEvent(Handle<AbstractCode> code,
                                     Handle<String> source) = 0;
  // Not handlified as this happens during GC. No allocation allowed.
  virtual void CodeMoveEvent(AbstractCode from, AbstractCode to) = 0;
  virtual void SharedFunctionInfoMoveEvent(Address from, Address to) = 0;
  virtual void NativeContextMoveEvent(Address from, Address to) = 0;
  virtual void CodeMovingGCEvent() = 0;
  virtual void CodeDisableOptEvent(Handle<AbstractCode> code,
                                   Handle<SharedFunctionInfo> shared) = 0;
  virtual void CodeDeoptEvent(Handle<Code> code, DeoptimizeKind kind,
                              Address pc, int fp_to_sp_delta) = 0;
  // These events can happen when 1. an assumption made by optimized code fails
  // or 2. a weakly embedded object dies.
  virtual void CodeDependencyChangeEvent(Handle<Code> code,
                                         Handle<SharedFunctionInfo> shared,
                                         const char* reason) = 0;
  // Called during GC shortly after any weak references to code objects are
  // cleared.
  virtual void WeakCodeClearEvent() = 0;

  virtual bool is_listening_to_code_events() { return false; }
};

// Dispatches code events to a set of registered listeners.
class LogEventDispatcher {
 public:
  using LogEventsAndTags = LogEventListener::LogEventsAndTags;

  LogEventDispatcher() = default;
  LogEventDispatcher(const LogEventDispatcher&) = delete;
  LogEventDispatcher& operator=(const LogEventDispatcher&) = delete;

  bool AddListener(LogEventListener* listener) {
    base::MutexGuard guard(&mutex_);
    auto position = std::find(listeners_.begin(), listeners_.end(), listener);
    if (position != listeners_.end()) return false;
    // Add the listener to the end and update the element
    listeners_.push_back(listener);
    if (!_is_listening_to_code_events) {
      _is_listening_to_code_events |= listener->is_listening_to_code_events();
    }
    DCHECK_EQ(_is_listening_to_code_events, IsListeningToCodeEvents());
    return true;
  }
  void RemoveListener(LogEventListener* listener) {
    base::MutexGuard guard(&mutex_);
    auto position = std::find(listeners_.begin(), listeners_.end(), listener);
    if (position == listeners_.end()) return;
    listeners_.erase(position);
    if (listener->is_listening_to_code_events()) {
      _is_listening_to_code_events = IsListeningToCodeEvents();
    }
    DCHECK_EQ(_is_listening_to_code_events, IsListeningToCodeEvents());
  }

  bool is_listening_to_code_events() const {
    DCHECK_EQ(_is_listening_to_code_events, IsListeningToCodeEvents());
    return _is_listening_to_code_events;
  }

  void CodeCreateEvent(LogEventsAndTags tag, Handle<AbstractCode> code,
                       const char* comment) {
    base::MutexGuard guard(&mutex_);
    for (auto listener : listeners_) {
      listener->CodeCreateEvent(tag, code, comment);
    }
  }
  void CodeCreateEvent(LogEventsAndTags tag, Handle<AbstractCode> code,
                       Handle<Name> name) {
    base::MutexGuard guard(&mutex_);
    for (auto listener : listeners_) {
      listener->CodeCreateEvent(tag, code, name);
    }
  }
  void CodeCreateEvent(LogEventsAndTags tag, Handle<AbstractCode> code,
                       Handle<SharedFunctionInfo> shared, Handle<Name> name) {
    base::MutexGuard guard(&mutex_);
    for (auto listener : listeners_) {
      listener->CodeCreateEvent(tag, code, shared, name);
    }
  }
  void CodeCreateEvent(LogEventsAndTags tag, Handle<AbstractCode> code,
                       Handle<SharedFunctionInfo> shared, Handle<Name> source,
                       int line, int column) {
    base::MutexGuard guard(&mutex_);
    for (auto listener : listeners_) {
      listener->CodeCreateEvent(tag, code, shared, source, line, column);
    }
  }
#if V8_ENABLE_WEBASSEMBLY
  void CodeCreateEvent(LogEventsAndTags tag, const wasm::WasmCode* code,
                       wasm::WasmName name, const char* source_url,
                       int code_offset, int script_id) {
    base::MutexGuard guard(&mutex_);
    for (auto listener : listeners_) {
      listener->CodeCreateEvent(tag, code, name, source_url, code_offset,
                                script_id);
    }
  }
#endif  // V8_ENABLE_WEBASSEMBLY
  void CallbackEvent(Handle<Name> name, Address entry_point) {
    base::MutexGuard guard(&mutex_);
    for (auto listener : listeners_) {
      listener->CallbackEvent(name, entry_point);
    }
  }
  void GetterCallbackEvent(Handle<Name> name, Address entry_point) {
    base::MutexGuard guard(&mutex_);
    for (auto listener : listeners_) {
      listener->GetterCallbackEvent(name, entry_point);
    }
  }
  void SetterCallbackEvent(Handle<Name> name, Address entry_point) {
    base::MutexGuard guard(&mutex_);
    for (auto listener : listeners_) {
      listener->SetterCallbackEvent(name, entry_point);
    }
  }
  void RegExpCodeCreateEvent(Handle<AbstractCode> code, Handle<String> source) {
    base::MutexGuard guard(&mutex_);
    for (auto listener : listeners_) {
      listener->RegExpCodeCreateEvent(code, source);
    }
  }
  void CodeMoveEvent(AbstractCode from, AbstractCode to) {
    base::MutexGuard guard(&mutex_);
    for (auto listener : listeners_) {
      listener->CodeMoveEvent(from, to);
    }
  }
  void SharedFunctionInfoMoveEvent(Address from, Address to) {
    base::MutexGuard guard(&mutex_);
    for (auto listener : listeners_) {
      listener->SharedFunctionInfoMoveEvent(from, to);
    }
  }
  void NativeContextMoveEvent(Address from, Address to) {
    base::MutexGuard guard(&mutex_);
    for (auto listener : listeners_) {
      listener->NativeContextMoveEvent(from, to);
    }
  }
  void CodeMovingGCEvent() {
    base::MutexGuard guard(&mutex_);
    for (auto listener : listeners_) {
      listener->CodeMovingGCEvent();
    }
  }
  void CodeDisableOptEvent(Handle<AbstractCode> code,
                           Handle<SharedFunctionInfo> shared) {
    base::MutexGuard guard(&mutex_);
    for (auto listener : listeners_) {
      listener->CodeDisableOptEvent(code, shared);
    }
  }
  void CodeDeoptEvent(Handle<Code> code, DeoptimizeKind kind, Address pc,
                      int fp_to_sp_delta) {
    base::MutexGuard guard(&mutex_);
    for (auto listener : listeners_) {
      listener->CodeDeoptEvent(code, kind, pc, fp_to_sp_delta);
    }
  }
  void CodeDependencyChangeEvent(Handle<Code> code,
                                 Handle<SharedFunctionInfo> sfi,
                                 const char* reason) {
    base::MutexGuard guard(&mutex_);
    for (auto listener : listeners_) {
      listener->CodeDependencyChangeEvent(code, sfi, reason);
    }
  }
  void WeakCodeClearEvent() {
    base::MutexGuard guard(&mutex_);
    for (auto listener : listeners_) {
      listener->WeakCodeClearEvent();
    }
  }

 private:
  bool IsListeningToCodeEvents() const {
    for (auto listener : listeners_) {
      if (listener->is_listening_to_code_events()) return true;
    }
    return false;
  }

  std::vector<LogEventListener*> listeners_;
  base::Mutex mutex_;
  bool _is_listening_to_code_events = false;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_LOGGING_CODE_EVENTS_H_
