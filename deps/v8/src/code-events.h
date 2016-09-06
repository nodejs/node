// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_CODE_EVENTS_H_
#define V8_CODE_EVENTS_H_

#include <unordered_set>

#include "src/globals.h"

namespace v8 {
namespace internal {

class AbstractCode;
class Name;
class SharedFunctionInfo;
class String;

#define LOG_EVENTS_AND_TAGS_LIST(V)                                      \
  V(CODE_CREATION_EVENT, "code-creation")                                \
  V(CODE_DISABLE_OPT_EVENT, "code-disable-optimization")                 \
  V(CODE_MOVE_EVENT, "code-move")                                        \
  V(CODE_DELETE_EVENT, "code-delete")                                    \
  V(CODE_MOVING_GC, "code-moving-gc")                                    \
  V(SHARED_FUNC_MOVE_EVENT, "sfi-move")                                  \
  V(SNAPSHOT_CODE_NAME_EVENT, "snapshot-code-name")                      \
  V(TICK_EVENT, "tick")                                                  \
  V(REPEAT_META_EVENT, "repeat")                                         \
  V(BUILTIN_TAG, "Builtin")                                              \
  V(CALL_DEBUG_BREAK_TAG, "CallDebugBreak")                              \
  V(CALL_DEBUG_PREPARE_STEP_IN_TAG, "CallDebugPrepareStepIn")            \
  V(CALL_INITIALIZE_TAG, "CallInitialize")                               \
  V(CALL_MEGAMORPHIC_TAG, "CallMegamorphic")                             \
  V(CALL_MISS_TAG, "CallMiss")                                           \
  V(CALL_NORMAL_TAG, "CallNormal")                                       \
  V(LOAD_INITIALIZE_TAG, "LoadInitialize")                               \
  V(LOAD_MEGAMORPHIC_TAG, "LoadMegamorphic")                             \
  V(STORE_INITIALIZE_TAG, "StoreInitialize")                             \
  V(STORE_GENERIC_TAG, "StoreGeneric")                                   \
  V(STORE_MEGAMORPHIC_TAG, "StoreMegamorphic")                           \
  V(KEYED_CALL_DEBUG_BREAK_TAG, "KeyedCallDebugBreak")                   \
  V(KEYED_CALL_DEBUG_PREPARE_STEP_IN_TAG, "KeyedCallDebugPrepareStepIn") \
  V(KEYED_CALL_INITIALIZE_TAG, "KeyedCallInitialize")                    \
  V(KEYED_CALL_MEGAMORPHIC_TAG, "KeyedCallMegamorphic")                  \
  V(KEYED_CALL_MISS_TAG, "KeyedCallMiss")                                \
  V(KEYED_CALL_NORMAL_TAG, "KeyedCallNormal")                            \
  V(CALLBACK_TAG, "Callback")                                            \
  V(EVAL_TAG, "Eval")                                                    \
  V(FUNCTION_TAG, "Function")                                            \
  V(HANDLER_TAG, "Handler")                                              \
  V(BYTECODE_HANDLER_TAG, "BytecodeHandler")                             \
  V(KEYED_LOAD_IC_TAG, "KeyedLoadIC")                                    \
  V(KEYED_LOAD_POLYMORPHIC_IC_TAG, "KeyedLoadPolymorphicIC")             \
  V(KEYED_EXTERNAL_ARRAY_LOAD_IC_TAG, "KeyedExternalArrayLoadIC")        \
  V(KEYED_STORE_IC_TAG, "KeyedStoreIC")                                  \
  V(KEYED_STORE_POLYMORPHIC_IC_TAG, "KeyedStorePolymorphicIC")           \
  V(KEYED_EXTERNAL_ARRAY_STORE_IC_TAG, "KeyedExternalArrayStoreIC")      \
  V(LAZY_COMPILE_TAG, "LazyCompile")                                     \
  V(CALL_IC_TAG, "CallIC")                                               \
  V(LOAD_IC_TAG, "LoadIC")                                               \
  V(LOAD_GLOBAL_IC_TAG, "LoadGlobalIC")                                  \
  V(LOAD_POLYMORPHIC_IC_TAG, "LoadPolymorphicIC")                        \
  V(REG_EXP_TAG, "RegExp")                                               \
  V(SCRIPT_TAG, "Script")                                                \
  V(STORE_IC_TAG, "StoreIC")                                             \
  V(STORE_POLYMORPHIC_IC_TAG, "StorePolymorphicIC")                      \
  V(STUB_TAG, "Stub")                                                    \
  V(NATIVE_FUNCTION_TAG, "Function")                                     \
  V(NATIVE_LAZY_COMPILE_TAG, "LazyCompile")                              \
  V(NATIVE_SCRIPT_TAG, "Script")
// Note that 'NATIVE_' cases for functions and scripts are mapped onto
// original tags when writing to the log.

#define PROFILE(the_isolate, Call) (the_isolate)->code_event_dispatcher()->Call;

class CodeEventListener {
 public:
#define DECLARE_ENUM(enum_item, _) enum_item,
  enum LogEventsAndTags {
    LOG_EVENTS_AND_TAGS_LIST(DECLARE_ENUM) NUMBER_OF_LOG_EVENTS
  };
#undef DECLARE_ENUM

  virtual ~CodeEventListener() {}

  virtual void CodeCreateEvent(LogEventsAndTags tag, AbstractCode* code,
                               const char* comment) = 0;
  virtual void CodeCreateEvent(LogEventsAndTags tag, AbstractCode* code,
                               Name* name) = 0;
  virtual void CodeCreateEvent(LogEventsAndTags tag, AbstractCode* code,
                               SharedFunctionInfo* shared, Name* name) = 0;
  virtual void CodeCreateEvent(LogEventsAndTags tag, AbstractCode* code,
                               SharedFunctionInfo* shared, Name* source,
                               int line, int column) = 0;
  virtual void CodeCreateEvent(LogEventsAndTags tag, AbstractCode* code,
                               int args_count) = 0;
  virtual void CallbackEvent(Name* name, Address entry_point) = 0;
  virtual void GetterCallbackEvent(Name* name, Address entry_point) = 0;
  virtual void SetterCallbackEvent(Name* name, Address entry_point) = 0;
  virtual void RegExpCodeCreateEvent(AbstractCode* code, String* source) = 0;
  virtual void CodeMoveEvent(AbstractCode* from, Address to) = 0;
  virtual void SharedFunctionInfoMoveEvent(Address from, Address to) = 0;
  virtual void CodeMovingGCEvent() = 0;
  virtual void CodeDisableOptEvent(AbstractCode* code,
                                   SharedFunctionInfo* shared) = 0;
  virtual void CodeDeoptEvent(Code* code, Address pc, int fp_to_sp_delta) = 0;
};

class CodeEventDispatcher {
 public:
  using LogEventsAndTags = CodeEventListener::LogEventsAndTags;

  CodeEventDispatcher() {}

  bool AddListener(CodeEventListener* listener) {
    return listeners_.insert(listener).second;
  }
  void RemoveListener(CodeEventListener* listener) {
    listeners_.erase(listener);
  }

#define CODE_EVENT_DISPATCH(code) \
  for (auto it = listeners_.begin(); it != listeners_.end(); ++it) (*it)->code

  void CodeCreateEvent(LogEventsAndTags tag, AbstractCode* code,
                       const char* comment) {
    CODE_EVENT_DISPATCH(CodeCreateEvent(tag, code, comment));
  }
  void CodeCreateEvent(LogEventsAndTags tag, AbstractCode* code, Name* name) {
    CODE_EVENT_DISPATCH(CodeCreateEvent(tag, code, name));
  }
  void CodeCreateEvent(LogEventsAndTags tag, AbstractCode* code,
                       SharedFunctionInfo* shared, Name* name) {
    CODE_EVENT_DISPATCH(CodeCreateEvent(tag, code, shared, name));
  }
  void CodeCreateEvent(LogEventsAndTags tag, AbstractCode* code,
                       SharedFunctionInfo* shared, Name* source, int line,
                       int column) {
    CODE_EVENT_DISPATCH(
        CodeCreateEvent(tag, code, shared, source, line, column));
  }
  void CodeCreateEvent(LogEventsAndTags tag, AbstractCode* code,
                       int args_count) {
    CODE_EVENT_DISPATCH(CodeCreateEvent(tag, code, args_count));
  }
  void CallbackEvent(Name* name, Address entry_point) {
    CODE_EVENT_DISPATCH(CallbackEvent(name, entry_point));
  }
  void GetterCallbackEvent(Name* name, Address entry_point) {
    CODE_EVENT_DISPATCH(GetterCallbackEvent(name, entry_point));
  }
  void SetterCallbackEvent(Name* name, Address entry_point) {
    CODE_EVENT_DISPATCH(SetterCallbackEvent(name, entry_point));
  }
  void RegExpCodeCreateEvent(AbstractCode* code, String* source) {
    CODE_EVENT_DISPATCH(RegExpCodeCreateEvent(code, source));
  }
  void CodeMoveEvent(AbstractCode* from, Address to) {
    CODE_EVENT_DISPATCH(CodeMoveEvent(from, to));
  }
  void SharedFunctionInfoMoveEvent(Address from, Address to) {
    CODE_EVENT_DISPATCH(SharedFunctionInfoMoveEvent(from, to));
  }
  void CodeMovingGCEvent() { CODE_EVENT_DISPATCH(CodeMovingGCEvent()); }
  void CodeDisableOptEvent(AbstractCode* code, SharedFunctionInfo* shared) {
    CODE_EVENT_DISPATCH(CodeDisableOptEvent(code, shared));
  }
  void CodeDeoptEvent(Code* code, Address pc, int fp_to_sp_delta) {
    CODE_EVENT_DISPATCH(CodeDeoptEvent(code, pc, fp_to_sp_delta));
  }
#undef CODE_EVENT_DISPATCH

 private:
  std::unordered_set<CodeEventListener*> listeners_;

  DISALLOW_COPY_AND_ASSIGN(CodeEventDispatcher);
};

}  // namespace internal
}  // namespace v8

#endif  // V8_CODE_EVENTS_H_
