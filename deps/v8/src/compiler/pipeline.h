// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_PIPELINE_H_
#define V8_COMPILER_PIPELINE_H_

#include <memory>

// Clients of this interface shouldn't depend on lots of compiler internals.
// Do not include anything from src/compiler here!
#include "src/common/globals.h"
#include "src/objects/code.h"

namespace v8 {
namespace internal {

struct AssemblerOptions;
class OptimizedCompilationInfo;
class TurbofanCompilationJob;
class ProfileDataFromFile;
class RegisterConfiguration;
struct WasmInliningPosition;

namespace wasm {
struct CompilationEnv;
struct WasmCompilationResult;
}  // namespace wasm

namespace compiler {

class CallDescriptor;
class Graph;
class InstructionSequence;
class JSGraph;
class JSHeapBroker;
class MachineGraph;
class Schedule;
class SourcePositionTable;
struct WasmCompilationData;

class Pipeline : public AllStatic {
 public:
  // Returns a new compilation job for the given JavaScript function.
  static V8_EXPORT_PRIVATE std::unique_ptr<TurbofanCompilationJob>
  NewCompilationJob(Isolate* isolate, Handle<JSFunction> function,
                    CodeKind code_kind, bool has_script,
                    BytecodeOffset osr_offset = BytecodeOffset::None());

  // Run the pipeline for the WebAssembly compilation info.
  static void GenerateCodeForWasmFunction(
      OptimizedCompilationInfo* info, wasm::CompilationEnv* env,
      WasmCompilationData& compilation_data, MachineGraph* mcgraph,
      CallDescriptor* call_descriptor,
      ZoneVector<WasmInliningPosition>* inlining_positions);

  // Run the pipeline on a machine graph and generate code.
  static wasm::WasmCompilationResult GenerateCodeForWasmNativeStub(
      CallDescriptor* call_descriptor, MachineGraph* mcgraph, CodeKind kind,
      const char* debug_name, const AssemblerOptions& assembler_options,
      SourcePositionTable* source_positions = nullptr);

  // Returns a new compilation job for a wasm heap stub.
  static std::unique_ptr<TurbofanCompilationJob> NewWasmHeapStubCompilationJob(
      Isolate* isolate, CallDescriptor* call_descriptor,
      std::unique_ptr<Zone> zone, Graph* graph, CodeKind kind,
      std::unique_ptr<char[]> debug_name, const AssemblerOptions& options);

  // Run the pipeline on a machine graph and generate code.
  static MaybeHandle<Code> GenerateCodeForCodeStub(
      Isolate* isolate, CallDescriptor* call_descriptor, Graph* graph,
      JSGraph* jsgraph, SourcePositionTable* source_positions, CodeKind kind,
      const char* debug_name, Builtin builtin, const AssemblerOptions& options,
      const ProfileDataFromFile* profile_data);

  // ---------------------------------------------------------------------------
  // The following methods are for testing purposes only. Avoid production use.
  // ---------------------------------------------------------------------------

  // Run the pipeline on JavaScript bytecode and generate code.  If requested,
  // hands out the heap broker on success, transferring its ownership to the
  // caller.
  V8_EXPORT_PRIVATE static MaybeHandle<Code> GenerateCodeForTesting(
      OptimizedCompilationInfo* info, Isolate* isolate,
      std::unique_ptr<JSHeapBroker>* out_broker = nullptr);

  // Run the pipeline on a machine graph and generate code. If {schedule} is
  // {nullptr}, then compute a new schedule for code generation.
  V8_EXPORT_PRIVATE static MaybeHandle<Code> GenerateCodeForTesting(
      OptimizedCompilationInfo* info, Isolate* isolate,
      CallDescriptor* call_descriptor, Graph* graph,
      const AssemblerOptions& options, Schedule* schedule = nullptr);

  // Run just the register allocator phases.
  V8_EXPORT_PRIVATE static void AllocateRegistersForTesting(
      const RegisterConfiguration* config, InstructionSequence* sequence,
      bool use_fast_register_allocator, bool run_verifier);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(Pipeline);
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_PIPELINE_H_
