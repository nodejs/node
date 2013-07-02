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


void HRangeAnalysisPhase::TraceRange(const char* msg, ...) {
  if (FLAG_trace_range) {
    va_list arguments;
    va_start(arguments, msg);
    OS::VPrint(msg, arguments);
    va_end(arguments);
  }
}


void HRangeAnalysisPhase::Analyze(HBasicBlock* block) {
  TraceRange("Analyzing block B%d\n", block->block_id());

  int last_changed_range = changed_ranges_.length() - 1;

  // Infer range based on control flow.
  if (block->predecessors()->length() == 1) {
    HBasicBlock* pred = block->predecessors()->first();
    if (pred->end()->IsCompareIDAndBranch()) {
      InferControlFlowRange(HCompareIDAndBranch::cast(pred->end()), block);
    }
  }

  // Process phi instructions.
  for (int i = 0; i < block->phis()->length(); ++i) {
    HPhi* phi = block->phis()->at(i);
    InferRange(phi);
  }

  // Go through all instructions of the current block.
  for (HInstructionIterator it(block); !it.Done(); it.Advance()) {
    InferRange(it.Current());
  }

  // Continue analysis in all dominated blocks.
  for (int i = 0; i < block->dominated_blocks()->length(); ++i) {
    Analyze(block->dominated_blocks()->at(i));
  }

  RollBackTo(last_changed_range);
}


void HRangeAnalysisPhase::InferControlFlowRange(HCompareIDAndBranch* test,
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
  for (int i = index + 1; i < changed_ranges_.length(); ++i) {
    changed_ranges_[i]->RemoveLastAddedRange();
  }
  changed_ranges_.Rewind(index + 1);
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


} }  // namespace v8::internal
