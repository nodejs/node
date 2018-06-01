// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_PROFILER_PROFILER_LISTENER_H_
#define V8_PROFILER_PROFILER_LISTENER_H_

#include <vector>

#include "src/code-events.h"
#include "src/profiler/profile-generator.h"

namespace v8 {
namespace internal {

class CodeEventsContainer;

class CodeEventObserver {
 public:
  virtual void CodeEventHandler(const CodeEventsContainer& evt_rec) = 0;
  virtual ~CodeEventObserver() {}
};

class ProfilerListener : public CodeEventListener {
 public:
  explicit ProfilerListener(Isolate* isolate);
  ~ProfilerListener() override;

  void CallbackEvent(Name* name, Address entry_point) override;
  void CodeCreateEvent(CodeEventListener::LogEventsAndTags tag,
                       AbstractCode* code, const char* comment) override;
  void CodeCreateEvent(CodeEventListener::LogEventsAndTags tag,
                       AbstractCode* code, Name* name) override;
  void CodeCreateEvent(CodeEventListener::LogEventsAndTags tag,
                       AbstractCode* code, SharedFunctionInfo* shared,
                       Name* script_name) override;
  void CodeCreateEvent(CodeEventListener::LogEventsAndTags tag,
                       AbstractCode* code, SharedFunctionInfo* shared,
                       Name* script_name, int line, int column) override;
  void CodeCreateEvent(CodeEventListener::LogEventsAndTags tag,
                       const wasm::WasmCode* code,
                       wasm::WasmName name) override;

  void CodeMovingGCEvent() override {}
  void CodeMoveEvent(AbstractCode* from, Address to) override;
  void CodeDisableOptEvent(AbstractCode* code,
                           SharedFunctionInfo* shared) override;
  void CodeDeoptEvent(Code* code, DeoptKind kind, Address pc,
                      int fp_to_sp_delta) override;
  void GetterCallbackEvent(Name* name, Address entry_point) override;
  void RegExpCodeCreateEvent(AbstractCode* code, String* source) override;
  void SetterCallbackEvent(Name* name, Address entry_point) override;
  void SharedFunctionInfoMoveEvent(Address from, Address to) override {}

  CodeEntry* NewCodeEntry(
      CodeEventListener::LogEventsAndTags tag, const char* name,
      const char* name_prefix = CodeEntry::kEmptyNamePrefix,
      const char* resource_name = CodeEntry::kEmptyResourceName,
      int line_number = v8::CpuProfileNode::kNoLineNumberInfo,
      int column_number = v8::CpuProfileNode::kNoColumnNumberInfo,
      std::unique_ptr<JITLineInfoTable> line_info = nullptr,
      Address instruction_start = nullptr);

  void AddObserver(CodeEventObserver* observer);
  void RemoveObserver(CodeEventObserver* observer);
  V8_INLINE bool HasObservers() { return !observers_.empty(); }

  const char* GetName(Name* name) {
    return function_and_resource_names_.GetName(name);
  }
  const char* GetName(int args_count) {
    return function_and_resource_names_.GetName(args_count);
  }
  const char* GetFunctionName(Name* name) {
    return function_and_resource_names_.GetFunctionName(name);
  }
  const char* GetFunctionName(const char* name) {
    return function_and_resource_names_.GetFunctionName(name);
  }
  size_t entries_count_for_test() const { return code_entries_.size(); }

 private:
  void RecordInliningInfo(CodeEntry* entry, AbstractCode* abstract_code);
  void RecordDeoptInlinedFrames(CodeEntry* entry, AbstractCode* abstract_code);
  Name* InferScriptName(Name* name, SharedFunctionInfo* info);
  V8_INLINE void DispatchCodeEvent(const CodeEventsContainer& evt_rec) {
    base::LockGuard<base::Mutex> guard(&mutex_);
    for (auto observer : observers_) {
      observer->CodeEventHandler(evt_rec);
    }
  }

  StringsStorage function_and_resource_names_;
  std::vector<std::unique_ptr<CodeEntry>> code_entries_;
  std::vector<CodeEventObserver*> observers_;
  base::Mutex mutex_;

  DISALLOW_COPY_AND_ASSIGN(ProfilerListener);
};

}  // namespace internal
}  // namespace v8

#endif  // V8_PROFILER_PROFILER_LISTENER_H_
