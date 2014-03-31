// Copyright 2013 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef V8_LITHIUM_CODEGEN_H_
#define V8_LITHIUM_CODEGEN_H_

#include "v8.h"

#include "compiler.h"

namespace v8 {
namespace internal {

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

  bool GenerateBody();
  virtual void GenerateBodyInstructionPre(LInstruction* instr) {}
  virtual void GenerateBodyInstructionPost(LInstruction* instr) {}

  virtual void EnsureSpaceForLazyDeopt(int space_needed) = 0;
  virtual void RecordAndWritePosition(int position) = 0;

  int GetNextEmittedBlock() const;

  void RegisterWeakObjectsInOptimizedCode(Handle<Code> code);

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
  int last_lazy_deopt_pc_;

  bool is_unused() const { return status_ == UNUSED; }
  bool is_generating() const { return status_ == GENERATING; }
  bool is_done() const { return status_ == DONE; }
  bool is_aborted() const { return status_ == ABORTED; }
};


} }  // namespace v8::internal

#endif  // V8_LITHIUM_CODEGEN_H_
