// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_LITHIUM_CODEGEN_H_
#define V8_LITHIUM_CODEGEN_H_

#include "src/v8.h"

#include "src/bailout-reason.h"
#include "src/compiler.h"
#include "src/deoptimizer.h"

namespace v8 {
namespace internal {

class LEnvironment;
class LInstruction;
class LPlatformChunk;

class LCodeGenBase BASE_EMBEDDED {
 public:
  LCodeGenBase(LChunk* chunk,
               MacroAssembler* assembler,
               CompilationInfo* info);
  virtual ~LCodeGenBase() {}

  // Simple accessors.
  MacroAssembler* masm() const { return masm_; }
  CompilationInfo* info() const { return info_; }
  Isolate* isolate() const { return info_->isolate(); }
  Factory* factory() const { return isolate()->factory(); }
  Heap* heap() const { return isolate()->heap(); }
  Zone* zone() const { return zone_; }
  LPlatformChunk* chunk() const { return chunk_; }
  HGraph* graph() const;

  void FPRINTF_CHECKING Comment(const char* format, ...);
  void DeoptComment(const Deoptimizer::DeoptInfo& deopt_info);
  static Deoptimizer::DeoptInfo MakeDeoptInfo(
      LInstruction* instr, Deoptimizer::DeoptReason deopt_reason);

  bool GenerateBody();
  virtual void GenerateBodyInstructionPre(LInstruction* instr) {}
  virtual void GenerateBodyInstructionPost(LInstruction* instr) {}

  virtual void EnsureSpaceForLazyDeopt(int space_needed) = 0;
  virtual void RecordAndWritePosition(int position) = 0;

  int GetNextEmittedBlock() const;

  void RegisterWeakObjectsInOptimizedCode(Handle<Code> code);

  void WriteTranslationFrame(LEnvironment* environment,
                             Translation* translation);
  int DefineDeoptimizationLiteral(Handle<Object> literal);

  // Check that an environment assigned via AssignEnvironment is actually being
  // used. Redundant assignments keep things alive longer than necessary, and
  // consequently lead to worse code, so it's important to minimize this.
  void CheckEnvironmentUsage();

 protected:
  enum Status {
    UNUSED,
    GENERATING,
    DONE,
    ABORTED
  };

  LPlatformChunk* const chunk_;
  MacroAssembler* const masm_;
  CompilationInfo* const info_;
  Zone* zone_;
  Status status_;
  int current_block_;
  int current_instruction_;
  const ZoneList<LInstruction*>* instructions_;
  ZoneList<Handle<Object> > deoptimization_literals_;
  int last_lazy_deopt_pc_;

  bool is_unused() const { return status_ == UNUSED; }
  bool is_generating() const { return status_ == GENERATING; }
  bool is_done() const { return status_ == DONE; }
  bool is_aborted() const { return status_ == ABORTED; }

  void Abort(BailoutReason reason);
  void Retry(BailoutReason reason);

  // Methods for code dependencies.
  void AddDeprecationDependency(Handle<Map> map);
  void AddStabilityDependency(Handle<Map> map);
};


} }  // namespace v8::internal

#endif  // V8_LITHIUM_CODEGEN_H_
