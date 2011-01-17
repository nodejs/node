// Copyright 2011 the V8 project authors. All rights reserved.
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

#ifndef V8_LITHIUM_H_
#define V8_LITHIUM_H_

#include "hydrogen.h"
#include "lithium-allocator.h"
#include "safepoint-table.h"

namespace v8 {
namespace internal {

class LCodeGen;
class Translation;

class LParallelMove : public ZoneObject {
 public:
  LParallelMove() : move_operands_(4) { }

  void AddMove(LOperand* from, LOperand* to) {
    move_operands_.Add(LMoveOperands(from, to));
  }

  bool IsRedundant() const;

  const ZoneList<LMoveOperands>* move_operands() const {
    return &move_operands_;
  }

  void PrintDataTo(StringStream* stream) const;

 private:
  ZoneList<LMoveOperands> move_operands_;
};


class LPointerMap: public ZoneObject {
 public:
  explicit LPointerMap(int position)
      : pointer_operands_(8), position_(position), lithium_position_(-1) { }

  const ZoneList<LOperand*>* operands() const { return &pointer_operands_; }
  int position() const { return position_; }
  int lithium_position() const { return lithium_position_; }

  void set_lithium_position(int pos) {
    ASSERT(lithium_position_ == -1);
    lithium_position_ = pos;
  }

  void RecordPointer(LOperand* op);
  void PrintTo(StringStream* stream);

 private:
  ZoneList<LOperand*> pointer_operands_;
  int position_;
  int lithium_position_;
};


class LEnvironment: public ZoneObject {
 public:
  LEnvironment(Handle<JSFunction> closure,
               int ast_id,
               int parameter_count,
               int argument_count,
               int value_count,
               LEnvironment* outer)
      : closure_(closure),
        arguments_stack_height_(argument_count),
        deoptimization_index_(Safepoint::kNoDeoptimizationIndex),
        translation_index_(-1),
        ast_id_(ast_id),
        parameter_count_(parameter_count),
        values_(value_count),
        representations_(value_count),
        spilled_registers_(NULL),
        spilled_double_registers_(NULL),
        outer_(outer) {
  }

  Handle<JSFunction> closure() const { return closure_; }
  int arguments_stack_height() const { return arguments_stack_height_; }
  int deoptimization_index() const { return deoptimization_index_; }
  int translation_index() const { return translation_index_; }
  int ast_id() const { return ast_id_; }
  int parameter_count() const { return parameter_count_; }
  LOperand** spilled_registers() const { return spilled_registers_; }
  LOperand** spilled_double_registers() const {
    return spilled_double_registers_;
  }
  const ZoneList<LOperand*>* values() const { return &values_; }
  LEnvironment* outer() const { return outer_; }

  void AddValue(LOperand* operand, Representation representation) {
    values_.Add(operand);
    representations_.Add(representation);
  }

  bool HasTaggedValueAt(int index) const {
    return representations_[index].IsTagged();
  }

  void Register(int deoptimization_index, int translation_index) {
    ASSERT(!HasBeenRegistered());
    deoptimization_index_ = deoptimization_index;
    translation_index_ = translation_index;
  }
  bool HasBeenRegistered() const {
    return deoptimization_index_ != Safepoint::kNoDeoptimizationIndex;
  }

  void SetSpilledRegisters(LOperand** registers,
                           LOperand** double_registers) {
    spilled_registers_ = registers;
    spilled_double_registers_ = double_registers;
  }

  void PrintTo(StringStream* stream);

 private:
  Handle<JSFunction> closure_;
  int arguments_stack_height_;
  int deoptimization_index_;
  int translation_index_;
  int ast_id_;
  int parameter_count_;
  ZoneList<LOperand*> values_;
  ZoneList<Representation> representations_;

  // Allocation index indexed arrays of spill slot operands for registers
  // that are also in spill slots at an OSR entry.  NULL for environments
  // that do not correspond to an OSR entry.
  LOperand** spilled_registers_;
  LOperand** spilled_double_registers_;

  LEnvironment* outer_;

  friend class LCodegen;
};

} }  // namespace v8::internal

#endif  // V8_LITHIUM_H_
