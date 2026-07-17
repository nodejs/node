// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_REGEXP_REGEXP_CODE_GENERATOR_H_
#define V8_REGEXP_REGEXP_CODE_GENERATOR_H_

#include "src/handles/handles.h"
#include "src/objects/fixed-array.h"
#include "src/regexp/regexp-bytecode-iterator.h"
#include "src/regexp/regexp-bytecodes.h"
#include "src/regexp/regexp-error.h"
#include "src/regexp/regexp-macro-assembler.h"
#include "src/utils/bit-vector.h"

namespace v8 {
namespace internal {

class RegExpCodeGenerator final {
 public:
  RegExpCodeGenerator(Isolate* isolate, RegExpMacroAssembler* masm,
                      DirectHandle<TrustedByteArray> bytecode);

  struct Result final {
    explicit Result(DirectHandle<Code> code) : code_(code) {}

    static Result UnsupportedBytecode() {
      return Result(RegExpError::kUnsupportedBytecode);
    }

    bool Succeeded() const { return error_ == RegExpError::kNone; }
    RegExpError error() const { return error_; }
    DirectHandle<Code> code() const { return code_; }

   private:
    explicit Result(RegExpError err) : error_(err) {}

    RegExpError error_ = RegExpError::kNone;
    DirectHandle<Code> code_;
  };

  V8_NODISCARD Result Assemble(DirectHandle<String> source, RegExpFlags flags);

 private:
  // Returns the value for |operand_id| of the current bytecode in the format
  // expected by the macro assembler.
  // E.g. converts an uint32_t bytecode offset to a Label*.
  template <typename Operands, typename Operands::Operand operand_id>
  auto GetArgumentValue();

  // Returns all the argument values of the current bytecode as a tuple.
  template <typename Operands>
  auto GetArgumentValuesAsTuple();

  // Visit all bytecodes before any code is emmited.
  // Allocates labels for all jump targets to support forward jumps.
  void PreVisitBytecodes();
  void VisitBytecodes();
  template <RegExpBytecode bc>
  void Visit();
  Label* GetLabel(uint32_t offset) const;
  MacroAssembler* NativeMasm();

  Isolate* isolate_;
  Zone zone_;
  RegExpMacroAssembler* masm_;
  DirectHandle<TrustedByteArray> bytecode_;
  RegExpBytecodeIterator iter_;
  // Zone allocated Array of Labels for each offset. Access is only valid for
  // offsets that are jump targets (indicated by jump_targets_).
  Label* labels_;
  // BitVector indicating if a specific offset is a valid jump target.
  // Labels are allocated for all offsets that are jump targets.
  BitVector jump_targets_;
  // BitVector indicating if a specific offset is an indirect jump target.
  // Indirect Jump Targets are all targets of `PushBacktrack`.
  // This is required for CFI.
  BitVector indirect_jump_targets_;
  bool has_unsupported_bytecode_;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_REGEXP_REGEXP_CODE_GENERATOR_H_
