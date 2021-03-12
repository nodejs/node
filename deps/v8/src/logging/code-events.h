// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_LOGGING_CODE_EVENTS_H_
#define V8_LOGGING_CODE_EVENTS_H_

#include <unordered_set>

#include "src/base/platform/mutex.h"
#include "src/common/globals.h"
#include "src/objects/code.h"
#include "src/objects/name.h"
#include "src/objects/shared-function-info.h"
#include "src/objects/string.h"
#include "src/utils/vector.h"

namespace v8 {
namespace internal {

class AbstractCode;
class Name;
class SharedFunctionInfo;
class String;

namespace wasm {
class WasmCode;
using WasmName = Vector<const char>;
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
  V(TICK_EVENT, tick)                                  \
  V(BYTECODE_FLUSH_EVENT, bytecode-flush)
// clang-format on

#define TAGS_LIST(V)                               \
  V(BUILTIN_TAG, Builtin)                          \
  V(CALLBACK_TAG, Callback)                        \
  V(EVAL_TAG, Eval)                                \
  V(FUNCTION_TAG, Function)                        \
  V(INTERPRETED_FUNCTION_TAG, InterpretedFunction) \
  V(HANDLER_TAG, Handler)                          \
  V(BYTECODE_HANDLER_TAG, BytecodeHandler)         \
  V(LAZY_COMPILE_TAG, LazyCompile)                 \
  V(REG_EXP_TAG, RegExp)                           \
  V(SCRIPT_TAG, Script)                            \
  V(STUB_TAG, Stub)                                \
  V(NATIVE_FUNCTION_TAG, Function)                 \
  V(NATIVE_LAZY_COMPILE_TAG, LazyCompile)          \
  V(NATIVE_SCRIPT_TAG, Script)
// Note that 'NATIVE_' cases for functions and scripts are mapped onto
// original tags when writing to the log.

#define LOG_EVENTS_AND_TAGS_LIST(V) \
  LOG_EVENTS_LIST(V)                \
  TAGS_LIST(V)

#define PROFILE(the_isolate, Call) (the_isolate)->code_event_dispatcher()->Call;

class CodeEventListener {
 public:
#define DECLARE_ENUM(enum_item, _) enum_item,
  enum LogEventsAndTags {
    LOG_EVENTS_AND_TAGS_LIST(DECLARE_ENUM) NUMBER_OF_LOG_EVENTS
  };
#undef DECLARE_ENUM

  virtual ~CodeEventListener() = default;

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
  virtual void CodeCreateEvent(LogEventsAndTags tag, const wasm::WasmCode* code,
                               wasm::WasmName name, const char* source_url,
                               int code_offset, int script_id) = 0;

  virtual void CallbackEvent(Handle<Name> name, Address entry_point) = 0;
  virtual void GetterCallbackEvent(Handle<Name> name, Address entry_point) = 0;
  virtual void SetterCallbackEvent(Handle<Name> name, Address entry_point) = 0;
  virtual void RegExpCodeCreateEvent(Handle<AbstractCode> code,
                                     Handle<String> source) = 0;
  // Not handlified as this happens during GC. No allocation allowed.
  virtual void CodeMoveEvent(AbstractCode from, AbstractCode to) = 0;
  virtual void SharedFunctionInfoMoveEvent(Address from, Address to) = 0;
  virtual void CodeMovingGCEvent() = 0;
  virtual void CodeDisableOptEvent(Handle<AbstractCode> code,
                                   Handle<SharedFunctionInfo> shared) = 0;
  virtual void CodeDeoptEvent(Handle<Code> code, DeoptimizeKind kind,
                              Address pc, int fp_to_sp_delta,
                              bool reuse_code) = 0;
  // These events can happen when 1. an assumption made by optimized code fails
  // or 2. a weakly embedded object dies.
  virtual void CodeDependencyChangeEvent(Handle<Code> code,
                                         Handle<SharedFunctionInfo> shared,
                                         const char* reason) = 0;
  // Invoked during GC. No allocation allowed.
  virtual void BytecodeFlushEvent(Address compiled_data_start) = 0;

  virtual bool is_listening_to_code_events() { return false; }
};

// Dispatches code events to a set of registered listeners.
class CodeEventDispatcher : public CodeEventListener {
 public:
  using LogEventsAndTags = CodeEventListener::LogEventsAndTags;

  CodeEventDispatcher() = default;
  CodeEventDispatcher(const CodeEventDispatcher&) = delete;
  CodeEventDispatcher& operator=(const CodeEventDispatcher&) = delete;

  bool AddListener(CodeEventListener* listener) {
    base::MutexGuard guard(&mutex_);
    return listeners_.insert(listener).second;
  }
  void RemoveListener(CodeEventListener* listener) {
    base::MutexGuard guard(&mutex_);
    listeners_.erase(listener);
  }
  bool IsListeningToCodeEvents() {
    for (auto it : listeners_) {
      if (it->is_listening_to_code_events()) {
        return true;
      }
    }
    return false;
  }

  void DispatchEventToListeners(
      std::function<void(CodeEventListener*)> callback) {
    base::MutexGuard guard(&mutex_);
    for (CodeEventListener* listener : listeners_) {
      callback(listener);
    }
  }

  void CodeCreateEvent(LogEventsAndTags tag, Handle<AbstractCode> code,
                       const char* comment) override {
    DispatchEventToListeners([=](CodeEventListener* listener) {
      listener->CodeCreateEvent(tag, code, comment);
    });
  }
  void CodeCreateEvent(LogEventsAndTags tag, Handle<AbstractCode> code,
                       Handle<Name> name) override {
    DispatchEventToListeners([=](CodeEventListener* listener) {
      listener->CodeCreateEvent(tag, code, name);
    });
  }
  void CodeCreateEvent(LogEventsAndTags tag, Handle<AbstractCode> code,
                       Handle<SharedFunctionInfo> shared,
                       Handle<Name> name) override {
    DispatchEventToListeners([=](CodeEventListener* listener) {
      listener->CodeCreateEvent(tag, code, shared, name);
    });
  }
  void CodeCreateEvent(LogEventsAndTags tag, Handle<AbstractCode> code,
                       Handle<SharedFunctionInfo> shared, Handle<Name> source,
                       int line, int column) override {
    DispatchEventToListeners([=](CodeEventListener* listener) {
      listener->CodeCreateEvent(tag, code, shared, source, line, column);
    });
  }
  void CodeCreateEvent(LogEventsAndTags tag, const wasm::WasmCode* code,
                       wasm::WasmName name, const char* source_url,
                       int code_offset, int script_id) override {
    DispatchEventToListeners([=](CodeEventListener* listener) {
      listener->CodeCreateEvent(tag, code, name, source_url, code_offset,
                                script_id);
    });
  }
  void CallbackEvent(Handle<Name> name, Address entry_point) override {
    DispatchEventToListeners([=](CodeEventListener* listener) {
      listener->CallbackEvent(name, entry_point);
    });
  }
  void GetterCallbackEvent(Handle<Name> name, Address entry_point) override {
    DispatchEventToListeners([=](CodeEventListener* listener) {
      listener->GetterCallbackEvent(name, entry_point);
    });
  }
  void SetterCallbackEvent(Handle<Name> name, Address entry_point) override {
    DispatchEventToListeners([=](CodeEventListener* listener) {
      listener->SetterCallbackEvent(name, entry_point);
    });
  }
  void RegExpCodeCreateEvent(Handle<AbstractCode> code,
                             Handle<String> source) override {
    DispatchEventToListeners([=](CodeEventListener* listener) {
      listener->RegExpCodeCreateEvent(code, source);
    });
  }
  void CodeMoveEvent(AbstractCode from, AbstractCode to) override {
    DispatchEventToListeners([=](CodeEventListener* listener) {
      listener->CodeMoveEvent(from, to);
    });
  }
  void SharedFunctionInfoMoveEvent(Address from, Address to) override {
    DispatchEventToListeners([=](CodeEventListener* listener) {
      listener->SharedFunctionInfoMoveEvent(from, to);
    });
  }
  void CodeMovingGCEvent() override {
    DispatchEventToListeners(
        [](CodeEventListener* listener) { listener->CodeMovingGCEvent(); });
  }
  void CodeDisableOptEvent(Handle<AbstractCode> code,
                           Handle<SharedFunctionInfo> shared) override {
    DispatchEventToListeners([=](CodeEventListener* listener) {
      listener->CodeDisableOptEvent(code, shared);
    });
  }
  void CodeDeoptEvent(Handle<Code> code, DeoptimizeKind kind, Address pc,
                      int fp_to_sp_delta, bool reuse_code) override {
    DispatchEventToListeners([=](CodeEventListener* listener) {
      listener->CodeDeoptEvent(code, kind, pc, fp_to_sp_delta, reuse_code);
    });
  }
  void CodeDependencyChangeEvent(Handle<Code> code,
                                 Handle<SharedFunctionInfo> sfi,
                                 const char* reason) override {
    DispatchEventToListeners([=](CodeEventListener* listener) {
      listener->CodeDependencyChangeEvent(code, sfi, reason);
    });
  }
  void BytecodeFlushEvent(Address compiled_data_start) override {
    DispatchEventToListeners([=](CodeEventListener* listener) {
      listener->BytecodeFlushEvent(compiled_data_start);
    });
  }

 private:
  std::unordered_set<CodeEventListener*> listeners_;
  base::Mutex mutex_;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_LOGGING_CODE_EVENTS_H_
