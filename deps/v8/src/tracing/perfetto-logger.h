// Copyright 2024 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_TRACING_PERFETTO_LOGGER_H_
#define V8_TRACING_PERFETTO_LOGGER_H_

#include "src/logging/code-events.h"

namespace v8 {
namespace internal {

class Isolate;

// Implementation that writes events to a Perfetto data source.
class PerfettoLogger : public LogEventListener {
 public:
  static void RegisterIsolate(Isolate* isolate);
  static void UnregisterIsolate(Isolate* isolate);
  static void OnCodeDataSourceStart();
  static void OnCodeDataSourceStop();

  explicit PerfettoLogger(Isolate* isolate);
  ~PerfettoLogger() override;

  void LogExistingCode();

  void CodeCreateEvent(CodeTag tag, DirectHandle<AbstractCode> code,
                       const char* name) override;
  void CodeCreateEvent(CodeTag tag, DirectHandle<AbstractCode> code,
                       DirectHandle<Name> name) override;
  void CodeCreateEvent(CodeTag tag, DirectHandle<AbstractCode> code,
                       DirectHandle<SharedFunctionInfo> shared,
                       DirectHandle<Name> script_name) override;
  void CodeCreateEvent(CodeTag tag, DirectHandle<AbstractCode> code,
                       DirectHandle<SharedFunctionInfo> shared,
                       DirectHandle<Name> script_name, int line,
                       int column) override;
#if V8_ENABLE_WEBASSEMBLY
  void CodeCreateEvent(CodeTag tag, const wasm::WasmCode* code,
                       wasm::WasmName name, const char* source_url,
                       int code_offset, int script_id) override;
#endif  // V8_ENABLE_WEBASSEMBLY

  void CallbackEvent(DirectHandle<Name> name, Address entry_point) override;
  void GetterCallbackEvent(DirectHandle<Name> name,
                           Address entry_point) override;
  void SetterCallbackEvent(DirectHandle<Name> name,
                           Address entry_point) override;
  void RegExpCodeCreateEvent(DirectHandle<AbstractCode> code,
                             DirectHandle<String> source,
                             RegExpFlags flags) override;
  void CodeMoveEvent(Tagged<InstructionStream> from,
                     Tagged<InstructionStream> to) override;
  void BytecodeMoveEvent(Tagged<BytecodeArray> from,
                         Tagged<BytecodeArray> to) override;
  void SharedFunctionInfoMoveEvent(Address from, Address to) override;
  void NativeContextMoveEvent(Address from, Address to) override;
  void CodeMovingGCEvent() override;
  void CodeDisableOptEvent(DirectHandle<AbstractCode> code,
                           DirectHandle<SharedFunctionInfo> shared) override;
  void CodeDeoptEvent(DirectHandle<Code> code, DeoptimizeKind kind, Address pc,
                      int fp_to_sp_delta) override;
  void CodeDependencyChangeEvent(DirectHandle<Code> code,
                                 DirectHandle<SharedFunctionInfo> shared,
                                 const char* reason) override;
  void WeakCodeClearEvent() override;
  bool is_listening_to_code_events() override;

 private:
  Isolate& isolate_;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_TRACING_PERFETTO_LOGGER_H_
