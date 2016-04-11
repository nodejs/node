// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_PIPELINE_H_
#define V8_COMPILER_PIPELINE_H_

// Clients of this interface shouldn't depend on lots of compiler internals.
// Do not include anything from src/compiler here!
#include "src/compiler.h"

namespace v8 {
namespace internal {

class RegisterConfiguration;

namespace compiler {

class CallDescriptor;
class Graph;
class InstructionSequence;
class Linkage;
class PipelineData;
class Schedule;

class Pipeline {
 public:
  explicit Pipeline(CompilationInfo* info) : info_(info) {}

  // Run the entire pipeline and generate a handle to a code object.
  Handle<Code> GenerateCode();

  // Run the pipeline on a machine graph and generate code. The {schedule} must
  // be valid, hence the given {graph} does not need to be schedulable.
  static Handle<Code> GenerateCodeForCodeStub(Isolate* isolate,
                                              CallDescriptor* call_descriptor,
                                              Graph* graph, Schedule* schedule,
                                              Code::Kind kind,
                                              const char* debug_name);

  // Run the pipeline on a machine graph and generate code. If {schedule} is
  // {nullptr}, then compute a new schedule for code generation.
  static Handle<Code> GenerateCodeForTesting(CompilationInfo* info,
                                             Graph* graph,
                                             Schedule* schedule = nullptr);

  // Run just the register allocator phases.
  static bool AllocateRegistersForTesting(const RegisterConfiguration* config,
                                          InstructionSequence* sequence,
                                          bool run_verifier);

  // Run the pipeline on a machine graph and generate code. If {schedule} is
  // {nullptr}, then compute a new schedule for code generation.
  static Handle<Code> GenerateCodeForTesting(CompilationInfo* info,
                                             CallDescriptor* call_descriptor,
                                             Graph* graph,
                                             Schedule* schedule = nullptr);

 private:
  CompilationInfo* info_;
  PipelineData* data_;

  // Helpers for executing pipeline phases.
  template <typename Phase>
  void Run();
  template <typename Phase, typename Arg0>
  void Run(Arg0 arg_0);

  CompilationInfo* info() const { return info_; }
  Isolate* isolate() { return info_->isolate(); }

  void BeginPhaseKind(const char* phase_kind);
  void RunPrintAndVerify(const char* phase, bool untyped = false);
  Handle<Code> ScheduleAndGenerateCode(CallDescriptor* call_descriptor);
  void AllocateRegisters(const RegisterConfiguration* config,
                         CallDescriptor* descriptor, bool run_verifier);
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_PIPELINE_H_
