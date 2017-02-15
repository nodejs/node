// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/type-hint-analyzer.h"

#include "src/assembler.h"
#include "src/code-stubs.h"
#include "src/ic/ic-state.h"
#include "src/type-hints.h"

namespace v8 {
namespace internal {
namespace compiler {

namespace {

BinaryOperationHint ToBinaryOperationHint(Token::Value op,
                                          BinaryOpICState::Kind kind) {
  switch (kind) {
    case BinaryOpICState::NONE:
      return BinaryOperationHint::kNone;
    case BinaryOpICState::SMI:
      return BinaryOperationHint::kSignedSmall;
    case BinaryOpICState::INT32:
      return (Token::IsTruncatingBinaryOp(op) && SmiValuesAre31Bits())
                 ? BinaryOperationHint::kNumberOrOddball
                 : BinaryOperationHint::kSigned32;
    case BinaryOpICState::NUMBER:
      return BinaryOperationHint::kNumberOrOddball;
    case BinaryOpICState::STRING:
      return BinaryOperationHint::kString;
    case BinaryOpICState::GENERIC:
      return BinaryOperationHint::kAny;
  }
  UNREACHABLE();
  return BinaryOperationHint::kNone;
}

CompareOperationHint ToCompareOperationHint(Token::Value op,
                                            CompareICState::State state) {
  switch (state) {
    case CompareICState::UNINITIALIZED:
      return CompareOperationHint::kNone;
    case CompareICState::SMI:
      return CompareOperationHint::kSignedSmall;
    case CompareICState::NUMBER:
      return Token::IsOrderedRelationalCompareOp(op)
                 ? CompareOperationHint::kNumberOrOddball
                 : CompareOperationHint::kNumber;
    case CompareICState::STRING:
    case CompareICState::INTERNALIZED_STRING:
    case CompareICState::UNIQUE_NAME:
    case CompareICState::RECEIVER:
    case CompareICState::KNOWN_RECEIVER:
    case CompareICState::BOOLEAN:
    case CompareICState::GENERIC:
      return CompareOperationHint::kAny;
  }
  UNREACHABLE();
  return CompareOperationHint::kNone;
}

}  // namespace

bool TypeHintAnalysis::GetBinaryOperationHint(TypeFeedbackId id,
                                              BinaryOperationHint* hint) const {
  auto i = infos_.find(id);
  if (i == infos_.end()) return false;
  Handle<Code> code = i->second;
  DCHECK_EQ(Code::BINARY_OP_IC, code->kind());
  BinaryOpICState state(code->GetIsolate(), code->extra_ic_state());
  *hint = ToBinaryOperationHint(state.op(), state.kind());
  return true;
}

bool TypeHintAnalysis::GetCompareOperationHint(
    TypeFeedbackId id, CompareOperationHint* hint) const {
  auto i = infos_.find(id);
  if (i == infos_.end()) return false;
  Handle<Code> code = i->second;
  DCHECK_EQ(Code::COMPARE_IC, code->kind());
  CompareICStub stub(code->stub_key(), code->GetIsolate());
  *hint = ToCompareOperationHint(stub.op(), stub.state());
  return true;
}

bool TypeHintAnalysis::GetToBooleanHints(TypeFeedbackId id,
                                         ToBooleanHints* hints) const {
  auto i = infos_.find(id);
  if (i == infos_.end()) return false;
  Handle<Code> code = i->second;
  DCHECK_EQ(Code::TO_BOOLEAN_IC, code->kind());
  ToBooleanICStub stub(code->GetIsolate(), code->extra_ic_state());
// TODO(bmeurer): Replace ToBooleanICStub::Types with ToBooleanHints.
#define ASSERT_COMPATIBLE(NAME, Name)         \
  STATIC_ASSERT(1 << ToBooleanICStub::NAME == \
                static_cast<int>(ToBooleanHint::k##Name))
  ASSERT_COMPATIBLE(UNDEFINED, Undefined);
  ASSERT_COMPATIBLE(BOOLEAN, Boolean);
  ASSERT_COMPATIBLE(NULL_TYPE, Null);
  ASSERT_COMPATIBLE(SMI, SmallInteger);
  ASSERT_COMPATIBLE(SPEC_OBJECT, Receiver);
  ASSERT_COMPATIBLE(STRING, String);
  ASSERT_COMPATIBLE(SYMBOL, Symbol);
  ASSERT_COMPATIBLE(HEAP_NUMBER, HeapNumber);
  ASSERT_COMPATIBLE(SIMD_VALUE, SimdValue);
#undef ASSERT_COMPATIBLE
  *hints = ToBooleanHints(stub.types().ToIntegral());
  return true;
}

TypeHintAnalysis* TypeHintAnalyzer::Analyze(Handle<Code> code) {
  DisallowHeapAllocation no_gc;
  TypeHintAnalysis::Infos infos(zone());
  Isolate* const isolate = code->GetIsolate();
  int const mask = RelocInfo::ModeMask(RelocInfo::CODE_TARGET_WITH_ID);
  for (RelocIterator it(*code, mask); !it.done(); it.next()) {
    RelocInfo* rinfo = it.rinfo();
    Address target_address = rinfo->target_address();
    Code* target = Code::GetCodeFromTargetAddress(target_address);
    switch (target->kind()) {
      case Code::BINARY_OP_IC:
      case Code::COMPARE_IC:
      case Code::TO_BOOLEAN_IC: {
        // Add this feedback to the {infos}.
        TypeFeedbackId id(static_cast<unsigned>(rinfo->data()));
        infos.insert(std::make_pair(id, handle(target, isolate)));
        break;
      }
      default:
        // Ignore the remaining code objects.
        break;
    }
  }
  return new (zone()) TypeHintAnalysis(infos, zone());
}


}  // namespace compiler
}  // namespace internal
}  // namespace v8
