// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_PROFILER_PROFILER_LISTENER_H_
#define V8_PROFILER_PROFILER_LISTENER_H_

#include <memory>
#include <vector>

#include "include/v8-profiler.h"
#include "src/logging/code-events.h"
#include "src/profiler/profile-generator.h"

namespace v8 {
namespace internal {

class CodeEventsContainer;
class CodeDeoptEventRecord;

class CodeEventObserver {
 public:
  virtual void CodeEventHandler(const CodeEventsContainer& evt_rec) = 0;
  virtual ~CodeEventObserver() = default;
};

class V8_EXPORT_PRIVATE ProfilerListener : public CodeEventListener {
 public:
  ProfilerListener(Isolate*, CodeEventObserver*,
                   CpuProfilingNamingMode mode = kDebugNaming);
  ~ProfilerListener() override;

  void CallbackEvent(Name name, Address entry_point) override;
  void CodeCreateEvent(CodeEventListener::LogEventsAndTags tag,
                       AbstractCode code, const char* comment) override;
  void CodeCreateEvent(CodeEventListener::LogEventsAndTags tag,
                       AbstractCode code, Name name) override;
  void CodeCreateEvent(CodeEventListener::LogEventsAndTags tag,
                       AbstractCode code, SharedFunctionInfo shared,
                       Name script_name) override;
  void CodeCreateEvent(CodeEventListener::LogEventsAndTags tag,
                       AbstractCode code, SharedFunctionInfo shared,
                       Name script_name, int line, int column) override;
  void CodeCreateEvent(CodeEventListener::LogEventsAndTags tag,
                       const wasm::WasmCode* code,
                       wasm::WasmName name) override;

  void CodeMovingGCEvent() override {}
  void CodeMoveEvent(AbstractCode from, AbstractCode to) override;
  void CodeDisableOptEvent(AbstractCode code,
                           SharedFunctionInfo shared) override;
  void CodeDeoptEvent(Code code, DeoptimizeKind kind, Address pc,
                      int fp_to_sp_delta) override;
  void GetterCallbackEvent(Name name, Address entry_point) override;
  void RegExpCodeCreateEvent(AbstractCode code, String source) override;
  void SetterCallbackEvent(Name name, Address entry_point) override;
  void SharedFunctionInfoMoveEvent(Address from, Address to) override {}

  const char* GetName(Name name) {
    return function_and_resource_names_.GetName(name);
  }
  const char* GetName(int args_count) {
    return function_and_resource_names_.GetName(args_count);
  }
  const char* GetName(const char* name) {
    return function_and_resource_names_.GetCopy(name);
  }
  const char* GetConsName(const char* prefix, Name name) {
    return function_and_resource_names_.GetConsName(prefix, name);
  }

  void set_observer(CodeEventObserver* observer) { observer_ = observer; }

 private:
  const char* GetFunctionName(SharedFunctionInfo);

  void AttachDeoptInlinedFrames(Code code, CodeDeoptEventRecord* rec);
  Name InferScriptName(Name name, SharedFunctionInfo info);
  V8_INLINE void DispatchCodeEvent(const CodeEventsContainer& evt_rec) {
    observer_->CodeEventHandler(evt_rec);
  }

  Isolate* isolate_;
  CodeEventObserver* observer_;
  StringsStorage function_and_resource_names_;
  const CpuProfilingNamingMode naming_mode_;

  DISALLOW_COPY_AND_ASSIGN(ProfilerListener);
};

}  // namespace internal
}  // namespace v8

#endif  // V8_PROFILER_PROFILER_LISTENER_H_
