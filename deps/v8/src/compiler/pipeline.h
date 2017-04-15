// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_PIPELINE_H_
#define V8_COMPILER_PIPELINE_H_

// Clients of this interface shouldn't depend on lots of compiler internals.
// Do not include anything from src/compiler here!
#include "src/globals.h"
#include "src/objects.h"
#include "src/zone/zone-containers.h"

namespace v8 {
namespace internal {

class CompilationInfo;
class CompilationJob;
class RegisterConfiguration;

namespace trap_handler {
struct ProtectedInstructionData;
}  // namespace trap_handler

namespace compiler {

class CallDescriptor;
class JSGraph;
class Graph;
class InstructionSequence;
class Schedule;
class SourcePositionTable;

class Pipeline : public AllStatic {
 public:
  // Returns a new compilation job for the given function.
  static CompilationJob* NewCompilationJob(Handle<JSFunction> function);

  // Returns a new compilation job for the WebAssembly compilation info.
  static CompilationJob* NewWasmCompilationJob(
      CompilationInfo* info, JSGraph* jsgraph, CallDescriptor* descriptor,
      SourcePositionTable* source_positions,
      ZoneVector<trap_handler::ProtectedInstructionData>*
          protected_instructions,
      bool wasm_origin);

  // Run the pipeline on a machine graph and generate code. The {schedule} must
  // be valid, hence the given {graph} does not need to be schedulable.
  static Handle<Code> GenerateCodeForCodeStub(Isolate* isolate,
                                              CallDescriptor* call_descriptor,
                                              Graph* graph, Schedule* schedule,
                                              Code::Flags flags,
                                              const char* debug_name);

  // Run the entire pipeline and generate a handle to a code object suitable for
  // testing.
  static Handle<Code> GenerateCodeForTesting(CompilationInfo* info);

  // Run the pipeline on a machine graph and generate code. If {schedule} is
  // {nullptr}, then compute a new schedule for code generation.
  static Handle<Code> GenerateCodeForTesting(CompilationInfo* info,
                                             Graph* graph,
                                             Schedule* schedule = nullptr);

  // Run just the register allocator phases.
  V8_EXPORT_PRIVATE static bool AllocateRegistersForTesting(
      const RegisterConfiguration* config, InstructionSequence* sequence,
      bool run_verifier);

  // Run the pipeline on a machine graph and generate code. If {schedule} is
  // {nullptr}, then compute a new schedule for code generation.
  static Handle<Code> GenerateCodeForTesting(
      CompilationInfo* info, CallDescriptor* call_descriptor, Graph* graph,
      Schedule* schedule = nullptr,
      SourcePositionTable* source_positions = nullptr);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(Pipeline);
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_PIPELINE_H_
