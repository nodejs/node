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

#include "hydrogen-range-analysis.h"

namespace v8 {
namespace internal {


class Pending {
 public:
  Pending(HBasicBlock* block, int last_changed_range)
      : block_(block), last_changed_range_(last_changed_range) {}

  HBasicBlock* block() const { return block_; }
  int last_changed_range() const { return last_changed_range_; }

 private:
  HBasicBlock* block_;
  int last_changed_range_;
};


void HRangeAnalysisPhase::TraceRange(const char* msg, ...) {
  if (FLAG_trace_range) {
    va_list arguments;
    va_start(arguments, msg);
    OS::VPrint(msg, arguments);
    va_end(arguments);
  }
}


void HRangeAnalysisPhase::Run() {
  HBasicBlock* block(graph()->entry_block());
  ZoneList<Pending> stack(graph()->blocks()->length(), zone());
  while (block != NULL) {
    TraceRange("Analyzing block B%d\n", block->block_id());

    // Infer range based on control flow.
    if (block->predecessors()->length() == 1) {
      HBasicBlock* pred = block->predecessors()->first();
      if (pred->end()->IsCompareNumericAndBranch()) {
        InferControlFlowRange(HCompareNumericAndBranch::cast(pred->end()),
                              block);
      }
    }

    // Process phi instructions.
    for (int i = 0; i < block->phis()->length(); ++i) {
      HPhi* phi = block->phis()->at(i);
      InferRange(phi);
    }

    // Go through all instructions of the current block.
    for (HInstructionIterator it(block); !it.Done(); it.Advance()) {
      HValue* value = it.Current();
      InferRange(value);

      // Compute the bailout-on-minus-zero flag.
      if (value->IsChange()) {
        HChange* instr = HChange::cast(value);
        // Propagate flags for negative zero checks upwards from conversions
        // int32-to-tagged and int32-to-double.
        Representation from = instr->value()->representation();
        ASSERT(from.Equals(instr->from()));
        if (from.IsSmiOrInteger32()) {
          ASSERT(instr->to().IsTagged() ||
                instr->to().IsDouble() ||
                instr->to().IsSmiOrInteger32());
          PropagateMinusZeroChecks(instr->value());
        }
      } else if (value->IsCompareMinusZeroAndBranch()) {
        HCompareMinusZeroAndBranch* instr =
            HCompareMinusZeroAndBranch::cast(value);
        if (instr->value()->representation().IsSmiOrInteger32()) {
          PropagateMinusZeroChecks(instr->value());
        }
      }
    }

    // Continue analysis in all dominated blocks.
    const ZoneList<HBasicBlock*>* dominated_blocks(block->dominated_blocks());
    if (!dominated_blocks->is_empty()) {
      // Continue with first dominated block, and push the
      // remaining blocks on the stack (in reverse order).
      int last_changed_range = changed_ranges_.length();
      for (int i = dominated_blocks->length() - 1; i > 0; --i) {
        stack.Add(Pending(dominated_blocks->at(i), last_changed_range), zone());
      }
      block = dominated_blocks->at(0);
    } else if (!stack.is_empty()) {
      // Pop next pending block from stack.
      Pending pending = stack.RemoveLast();
      RollBackTo(pending.last_changed_range());
      block = pending.block();
    } else {
      // All blocks done.
      block = NULL;
    }
  }
}


void HRangeAnalysisPhase::InferControlFlowRange(HCompareNumericAndBranch* test,
                                                HBasicBlock* dest) {
  ASSERT((test->FirstSuccessor() == dest) == (test->SecondSuccessor() != dest));
  if (test->representation().IsSmiOrInteger32()) {
    Token::Value op = test->token();
    if (test->SecondSuccessor() == dest) {
      op = Token::NegateCompareOp(op);
    }
    Token::Value inverted_op = Token::ReverseCompareOp(op);
    UpdateControlFlowRange(op, test->left(), test->right());
    UpdateControlFlowRange(inverted_op, test->right(), test->left());
  }
}


// We know that value [op] other. Use this information to update the range on
// value.
void HRangeAnalysisPhase::UpdateControlFlowRange(Token::Value op,
                                                 HValue* value,
                                                 HValue* other) {
  Range temp_range;
  Range* range = other->range() != NULL ? other->range() : &temp_range;
  Range* new_range = NULL;

  TraceRange("Control flow range infer %d %s %d\n",
             value->id(),
             Token::Name(op),
             other->id());

  if (op == Token::EQ || op == Token::EQ_STRICT) {
    // The same range has to apply for value.
    new_range = range->Copy(graph()->zone());
  } else if (op == Token::LT || op == Token::LTE) {
    new_range = range->CopyClearLower(graph()->zone());
    if (op == Token::LT) {
      new_range->AddConstant(-1);
    }
  } else if (op == Token::GT || op == Token::GTE) {
    new_range = range->CopyClearUpper(graph()->zone());
    if (op == Token::GT) {
      new_range->AddConstant(1);
    }
  }

  if (new_range != NULL && !new_range->IsMostGeneric()) {
    AddRange(value, new_range);
  }
}


void HRangeAnalysisPhase::InferRange(HValue* value) {
  ASSERT(!value->HasRange());
  if (!value->representation().IsNone()) {
    value->ComputeInitialRange(graph()->zone());
    Range* range = value->range();
    TraceRange("Initial inferred range of %d (%s) set to [%d,%d]\n",
               value->id(),
               value->Mnemonic(),
               range->lower(),
               range->upper());
  }
}


void HRangeAnalysisPhase::RollBackTo(int index) {
  ASSERT(index <= changed_ranges_.length());
  for (int i = index; i < changed_ranges_.length(); ++i) {
    changed_ranges_[i]->RemoveLastAddedRange();
  }
  changed_ranges_.Rewind(index);
}


void HRangeAnalysisPhase::AddRange(HValue* value, Range* range) {
  Range* original_range = value->range();
  value->AddNewRange(range, graph()->zone());
  changed_ranges_.Add(value, zone());
  Range* new_range = value->range();
  TraceRange("Updated range of %d set to [%d,%d]\n",
             value->id(),
             new_range->lower(),
             new_range->upper());
  if (original_range != NULL) {
    TraceRange("Original range was [%d,%d]\n",
               original_range->lower(),
               original_range->upper());
  }
  TraceRange("New information was [%d,%d]\n",
             range->lower(),
             range->upper());
}


void HRangeAnalysisPhase::PropagateMinusZeroChecks(HValue* value) {
  ASSERT(worklist_.is_empty());
  ASSERT(in_worklist_.IsEmpty());

  AddToWorklist(value);
  while (!worklist_.is_empty()) {
    value = worklist_.RemoveLast();

    if (value->IsPhi()) {
      // For phis, we must propagate the check to all of its inputs.
      HPhi* phi = HPhi::cast(value);
      for (int i = 0; i < phi->OperandCount(); ++i) {
        AddToWorklist(phi->OperandAt(i));
      }
    } else if (value->IsUnaryMathOperation()) {
      HUnaryMathOperation* instr = HUnaryMathOperation::cast(value);
      if (instr->representation().IsSmiOrInteger32() &&
          !instr->value()->representation().Equals(instr->representation())) {
        if (instr->value()->range() == NULL ||
            instr->value()->range()->CanBeMinusZero()) {
          instr->SetFlag(HValue::kBailoutOnMinusZero);
        }
      }
      if (instr->RequiredInputRepresentation(0).IsSmiOrInteger32() &&
          instr->representation().Equals(
              instr->RequiredInputRepresentation(0))) {
        AddToWorklist(instr->value());
      }
    } else if (value->IsChange()) {
      HChange* instr = HChange::cast(value);
      if (!instr->from().IsSmiOrInteger32() &&
          !instr->CanTruncateToInt32() &&
          (instr->value()->range() == NULL ||
           instr->value()->range()->CanBeMinusZero())) {
        instr->SetFlag(HValue::kBailoutOnMinusZero);
      }
    } else if (value->IsForceRepresentation()) {
      HForceRepresentation* instr = HForceRepresentation::cast(value);
      AddToWorklist(instr->value());
    } else if (value->IsMod()) {
      HMod* instr = HMod::cast(value);
      if (instr->range() == NULL || instr->range()->CanBeMinusZero()) {
        instr->SetFlag(HValue::kBailoutOnMinusZero);
        AddToWorklist(instr->left());
      }
    } else if (value->IsDiv() || value->IsMul()) {
      HBinaryOperation* instr = HBinaryOperation::cast(value);
      if (instr->range() == NULL || instr->range()->CanBeMinusZero()) {
        instr->SetFlag(HValue::kBailoutOnMinusZero);
      }
      AddToWorklist(instr->right());
      AddToWorklist(instr->left());
    } else if (value->IsMathFloorOfDiv()) {
      HMathFloorOfDiv* instr = HMathFloorOfDiv::cast(value);
      instr->SetFlag(HValue::kBailoutOnMinusZero);
    } else if (value->IsAdd() || value->IsSub()) {
      HBinaryOperation* instr = HBinaryOperation::cast(value);
      if (instr->range() == NULL || instr->range()->CanBeMinusZero()) {
        // Propagate to the left argument. If the left argument cannot be -0,
        // then the result of the add/sub operation cannot be either.
        AddToWorklist(instr->left());
      }
    } else if (value->IsMathMinMax()) {
      HMathMinMax* instr = HMathMinMax::cast(value);
      AddToWorklist(instr->right());
      AddToWorklist(instr->left());
    }
  }

  in_worklist_.Clear();
  ASSERT(in_worklist_.IsEmpty());
  ASSERT(worklist_.is_empty());
}


} }  // namespace v8::internal
