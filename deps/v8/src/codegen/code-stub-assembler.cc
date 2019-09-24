// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/codegen/code-stub-assembler.h"

#include "include/v8-internal.h"
#include "src/base/macros.h"
#include "src/codegen/code-factory.h"
#include "src/common/globals.h"
#include "src/execution/frames-inl.h"
#include "src/execution/frames.h"
#include "src/heap/heap-inl.h"  // For Page/MemoryChunk. TODO(jkummerow): Drop.
#include "src/logging/counters.h"
#include "src/objects/api-callbacks.h"
#include "src/objects/cell.h"
#include "src/objects/descriptor-array.h"
#include "src/objects/function-kind.h"
#include "src/objects/heap-number.h"
#include "src/objects/oddball.h"
#include "src/objects/ordered-hash-table-inl.h"
#include "src/objects/property-cell.h"
#include "src/wasm/wasm-objects.h"

namespace v8 {
namespace internal {

using compiler::Node;
template <class T>
using TNode = compiler::TNode<T>;
template <class T>
using SloppyTNode = compiler::SloppyTNode<T>;

CodeStubAssembler::CodeStubAssembler(compiler::CodeAssemblerState* state)
    : compiler::CodeAssembler(state),
      TorqueGeneratedExportedMacrosAssembler(state) {
  if (DEBUG_BOOL && FLAG_csa_trap_on_node != nullptr) {
    HandleBreakOnNode();
  }
}

void CodeStubAssembler::HandleBreakOnNode() {
  // FLAG_csa_trap_on_node should be in a form "STUB,NODE" where STUB is a
  // string specifying the name of a stub and NODE is number specifying node id.
  const char* name = state()->name();
  size_t name_length = strlen(name);
  if (strncmp(FLAG_csa_trap_on_node, name, name_length) != 0) {
    // Different name.
    return;
  }
  size_t option_length = strlen(FLAG_csa_trap_on_node);
  if (option_length < name_length + 2 ||
      FLAG_csa_trap_on_node[name_length] != ',') {
    // Option is too short.
    return;
  }
  const char* start = &FLAG_csa_trap_on_node[name_length + 1];
  char* end;
  int node_id = static_cast<int>(strtol(start, &end, 10));
  if (start == end) {
    // Bad node id.
    return;
  }
  BreakOnNode(node_id);
}

void CodeStubAssembler::Assert(const BranchGenerator& branch,
                               const char* message, const char* file, int line,
                               std::initializer_list<ExtraNode> extra_nodes) {
#if defined(DEBUG)
  if (FLAG_debug_code) {
    Check(branch, message, file, line, extra_nodes);
  }
#endif
}

void CodeStubAssembler::Assert(const NodeGenerator& condition_body,
                               const char* message, const char* file, int line,
                               std::initializer_list<ExtraNode> extra_nodes) {
#if defined(DEBUG)
  if (FLAG_debug_code) {
    Check(condition_body, message, file, line, extra_nodes);
  }
#endif
}

void CodeStubAssembler::Assert(SloppyTNode<Word32T> condition_node,
                               const char* message, const char* file, int line,
                               std::initializer_list<ExtraNode> extra_nodes) {
#if defined(DEBUG)
  if (FLAG_debug_code) {
    Check(condition_node, message, file, line, extra_nodes);
  }
#endif
}

void CodeStubAssembler::Check(const BranchGenerator& branch,
                              const char* message, const char* file, int line,
                              std::initializer_list<ExtraNode> extra_nodes) {
  Label ok(this);
  Label not_ok(this, Label::kDeferred);
  if (message != nullptr && FLAG_code_comments) {
    Comment("[ Assert: ", message);
  } else {
    Comment("[ Assert");
  }
  branch(&ok, &not_ok);

  BIND(&not_ok);
  FailAssert(message, file, line, extra_nodes);

  BIND(&ok);
  Comment("] Assert");
}

void CodeStubAssembler::Check(const NodeGenerator& condition_body,
                              const char* message, const char* file, int line,
                              std::initializer_list<ExtraNode> extra_nodes) {
  BranchGenerator branch = [=](Label* ok, Label* not_ok) {
    Node* condition = condition_body();
    DCHECK_NOT_NULL(condition);
    Branch(condition, ok, not_ok);
  };

  Check(branch, message, file, line, extra_nodes);
}

void CodeStubAssembler::Check(SloppyTNode<Word32T> condition_node,
                              const char* message, const char* file, int line,
                              std::initializer_list<ExtraNode> extra_nodes) {
  BranchGenerator branch = [=](Label* ok, Label* not_ok) {
    Branch(condition_node, ok, not_ok);
  };

  Check(branch, message, file, line, extra_nodes);
}

void CodeStubAssembler::FastCheck(TNode<BoolT> condition) {
  Label ok(this), not_ok(this, Label::kDeferred);
  Branch(condition, &ok, &not_ok);
  BIND(&not_ok);
  {
    DebugBreak();
    Goto(&ok);
  }
  BIND(&ok);
}

void CodeStubAssembler::FailAssert(
    const char* message, const char* file, int line,
    std::initializer_list<ExtraNode> extra_nodes) {
  DCHECK_NOT_NULL(message);
  EmbeddedVector<char, 1024> chars;
  if (file != nullptr) {
    SNPrintF(chars, "%s [%s:%d]", message, file, line);
    message = chars.begin();
  }
  TNode<String> message_node = StringConstant(message);

#ifdef DEBUG
  // Only print the extra nodes in debug builds.
  for (auto& node : extra_nodes) {
    CallRuntime(Runtime::kPrintWithNameForAssert, SmiConstant(0),
                StringConstant(node.second), node.first);
  }
#endif

  AbortCSAAssert(message_node);
  Unreachable();
}

Node* CodeStubAssembler::SelectImpl(TNode<BoolT> condition,
                                    const NodeGenerator& true_body,
                                    const NodeGenerator& false_body,
                                    MachineRepresentation rep) {
  VARIABLE(value, rep);
  Label vtrue(this), vfalse(this), end(this);
  Branch(condition, &vtrue, &vfalse);

  BIND(&vtrue);
  {
    value.Bind(true_body());
    Goto(&end);
  }
  BIND(&vfalse);
  {
    value.Bind(false_body());
    Goto(&end);
  }

  BIND(&end);
  return value.value();
}

TNode<Int32T> CodeStubAssembler::SelectInt32Constant(
    SloppyTNode<BoolT> condition, int true_value, int false_value) {
  return SelectConstant<Int32T>(condition, Int32Constant(true_value),
                                Int32Constant(false_value));
}

TNode<IntPtrT> CodeStubAssembler::SelectIntPtrConstant(
    SloppyTNode<BoolT> condition, int true_value, int false_value) {
  return SelectConstant<IntPtrT>(condition, IntPtrConstant(true_value),
                                 IntPtrConstant(false_value));
}

TNode<Oddball> CodeStubAssembler::SelectBooleanConstant(
    SloppyTNode<BoolT> condition) {
  return SelectConstant<Oddball>(condition, TrueConstant(), FalseConstant());
}

TNode<Smi> CodeStubAssembler::SelectSmiConstant(SloppyTNode<BoolT> condition,
                                                Smi true_value,
                                                Smi false_value) {
  return SelectConstant<Smi>(condition, SmiConstant(true_value),
                             SmiConstant(false_value));
}

TNode<Object> CodeStubAssembler::NoContextConstant() {
  return SmiConstant(Context::kNoContext);
}

#define HEAP_CONSTANT_ACCESSOR(rootIndexName, rootAccessorName, name)        \
  compiler::TNode<std::remove_pointer<std::remove_reference<decltype(        \
      std::declval<Heap>().rootAccessorName())>::type>::type>                \
      CodeStubAssembler::name##Constant() {                                  \
    return UncheckedCast<std::remove_pointer<std::remove_reference<decltype( \
        std::declval<Heap>().rootAccessorName())>::type>::type>(             \
        LoadRoot(RootIndex::k##rootIndexName));                              \
  }
HEAP_MUTABLE_IMMOVABLE_OBJECT_LIST(HEAP_CONSTANT_ACCESSOR)
#undef HEAP_CONSTANT_ACCESSOR

#define HEAP_CONSTANT_ACCESSOR(rootIndexName, rootAccessorName, name)        \
  compiler::TNode<std::remove_pointer<std::remove_reference<decltype(        \
      std::declval<ReadOnlyRoots>().rootAccessorName())>::type>::type>       \
      CodeStubAssembler::name##Constant() {                                  \
    return UncheckedCast<std::remove_pointer<std::remove_reference<decltype( \
        std::declval<ReadOnlyRoots>().rootAccessorName())>::type>::type>(    \
        LoadRoot(RootIndex::k##rootIndexName));                              \
  }
HEAP_IMMUTABLE_IMMOVABLE_OBJECT_LIST(HEAP_CONSTANT_ACCESSOR)
#undef HEAP_CONSTANT_ACCESSOR

#define HEAP_CONSTANT_TEST(rootIndexName, rootAccessorName, name) \
  compiler::TNode<BoolT> CodeStubAssembler::Is##name(             \
      SloppyTNode<Object> value) {                                \
    return TaggedEqual(value, name##Constant());                  \
  }                                                               \
  compiler::TNode<BoolT> CodeStubAssembler::IsNot##name(          \
      SloppyTNode<Object> value) {                                \
    return TaggedNotEqual(value, name##Constant());               \
  }
HEAP_IMMOVABLE_OBJECT_LIST(HEAP_CONSTANT_TEST)
#undef HEAP_CONSTANT_TEST

TNode<BInt> CodeStubAssembler::BIntConstant(int value) {
#if defined(BINT_IS_SMI)
  return SmiConstant(value);
#elif defined(BINT_IS_INTPTR)
  return IntPtrConstant(value);
#else
#error Unknown architecture.
#endif
}

Node* CodeStubAssembler::IntPtrOrSmiConstant(int value, ParameterMode mode) {
  if (mode == SMI_PARAMETERS) {
    return SmiConstant(value);
  } else {
    DCHECK_EQ(INTPTR_PARAMETERS, mode);
    return IntPtrConstant(value);
  }
}

TNode<BoolT> CodeStubAssembler::IntPtrOrSmiEqual(Node* left, Node* right,
                                                 ParameterMode mode) {
  if (mode == SMI_PARAMETERS) {
    return SmiEqual(CAST(left), CAST(right));
  } else {
    DCHECK_EQ(INTPTR_PARAMETERS, mode);
    return IntPtrEqual(UncheckedCast<IntPtrT>(left),
                       UncheckedCast<IntPtrT>(right));
  }
}

TNode<BoolT> CodeStubAssembler::IntPtrOrSmiNotEqual(Node* left, Node* right,
                                                    ParameterMode mode) {
  if (mode == SMI_PARAMETERS) {
    return SmiNotEqual(CAST(left), CAST(right));
  } else {
    DCHECK_EQ(INTPTR_PARAMETERS, mode);
    return WordNotEqual(UncheckedCast<IntPtrT>(left),
                        UncheckedCast<IntPtrT>(right));
  }
}

bool CodeStubAssembler::IsIntPtrOrSmiConstantZero(Node* test,
                                                  ParameterMode mode) {
  int32_t constant_test;
  Smi smi_test;
  if (mode == INTPTR_PARAMETERS) {
    if (ToInt32Constant(test, &constant_test) && constant_test == 0) {
      return true;
    }
  } else {
    DCHECK_EQ(mode, SMI_PARAMETERS);
    if (ToSmiConstant(test, &smi_test) && smi_test.value() == 0) {
      return true;
    }
  }
  return false;
}

bool CodeStubAssembler::TryGetIntPtrOrSmiConstantValue(Node* maybe_constant,
                                                       int* value,
                                                       ParameterMode mode) {
  int32_t int32_constant;
  if (mode == INTPTR_PARAMETERS) {
    if (ToInt32Constant(maybe_constant, &int32_constant)) {
      *value = int32_constant;
      return true;
    }
  } else {
    DCHECK_EQ(mode, SMI_PARAMETERS);
    Smi smi_constant;
    if (ToSmiConstant(maybe_constant, &smi_constant)) {
      *value = Smi::ToInt(smi_constant);
      return true;
    }
  }
  return false;
}

TNode<IntPtrT> CodeStubAssembler::IntPtrRoundUpToPowerOfTwo32(
    TNode<IntPtrT> value) {
  Comment("IntPtrRoundUpToPowerOfTwo32");
  CSA_ASSERT(this, UintPtrLessThanOrEqual(value, IntPtrConstant(0x80000000u)));
  value = Signed(IntPtrSub(value, IntPtrConstant(1)));
  for (int i = 1; i <= 16; i *= 2) {
    value = Signed(WordOr(value, WordShr(value, IntPtrConstant(i))));
  }
  return Signed(IntPtrAdd(value, IntPtrConstant(1)));
}

Node* CodeStubAssembler::MatchesParameterMode(Node* value, ParameterMode mode) {
  if (mode == SMI_PARAMETERS) {
    return TaggedIsSmi(value);
  } else {
    return Int32Constant(1);
  }
}

TNode<BoolT> CodeStubAssembler::WordIsPowerOfTwo(SloppyTNode<IntPtrT> value) {
  // value && !(value & (value - 1))
  return IntPtrEqual(
      Select<IntPtrT>(
          IntPtrEqual(value, IntPtrConstant(0)),
          [=] { return IntPtrConstant(1); },
          [=] { return WordAnd(value, IntPtrSub(value, IntPtrConstant(1))); }),
      IntPtrConstant(0));
}

TNode<Float64T> CodeStubAssembler::Float64Round(SloppyTNode<Float64T> x) {
  TNode<Float64T> one = Float64Constant(1.0);
  TNode<Float64T> one_half = Float64Constant(0.5);

  Label return_x(this);

  // Round up {x} towards Infinity.
  VARIABLE(var_x, MachineRepresentation::kFloat64, Float64Ceil(x));

  GotoIf(Float64LessThanOrEqual(Float64Sub(var_x.value(), one_half), x),
         &return_x);
  var_x.Bind(Float64Sub(var_x.value(), one));
  Goto(&return_x);

  BIND(&return_x);
  return TNode<Float64T>::UncheckedCast(var_x.value());
}

TNode<Float64T> CodeStubAssembler::Float64Ceil(SloppyTNode<Float64T> x) {
  if (IsFloat64RoundUpSupported()) {
    return Float64RoundUp(x);
  }

  TNode<Float64T> one = Float64Constant(1.0);
  TNode<Float64T> zero = Float64Constant(0.0);
  TNode<Float64T> two_52 = Float64Constant(4503599627370496.0E0);
  TNode<Float64T> minus_two_52 = Float64Constant(-4503599627370496.0E0);

  VARIABLE(var_x, MachineRepresentation::kFloat64, x);
  Label return_x(this), return_minus_x(this);

  // Check if {x} is greater than zero.
  Label if_xgreaterthanzero(this), if_xnotgreaterthanzero(this);
  Branch(Float64GreaterThan(x, zero), &if_xgreaterthanzero,
         &if_xnotgreaterthanzero);

  BIND(&if_xgreaterthanzero);
  {
    // Just return {x} unless it's in the range ]0,2^52[.
    GotoIf(Float64GreaterThanOrEqual(x, two_52), &return_x);

    // Round positive {x} towards Infinity.
    var_x.Bind(Float64Sub(Float64Add(two_52, x), two_52));
    GotoIfNot(Float64LessThan(var_x.value(), x), &return_x);
    var_x.Bind(Float64Add(var_x.value(), one));
    Goto(&return_x);
  }

  BIND(&if_xnotgreaterthanzero);
  {
    // Just return {x} unless it's in the range ]-2^52,0[
    GotoIf(Float64LessThanOrEqual(x, minus_two_52), &return_x);
    GotoIfNot(Float64LessThan(x, zero), &return_x);

    // Round negated {x} towards Infinity and return the result negated.
    TNode<Float64T> minus_x = Float64Neg(x);
    var_x.Bind(Float64Sub(Float64Add(two_52, minus_x), two_52));
    GotoIfNot(Float64GreaterThan(var_x.value(), minus_x), &return_minus_x);
    var_x.Bind(Float64Sub(var_x.value(), one));
    Goto(&return_minus_x);
  }

  BIND(&return_minus_x);
  var_x.Bind(Float64Neg(var_x.value()));
  Goto(&return_x);

  BIND(&return_x);
  return TNode<Float64T>::UncheckedCast(var_x.value());
}

TNode<Float64T> CodeStubAssembler::Float64Floor(SloppyTNode<Float64T> x) {
  if (IsFloat64RoundDownSupported()) {
    return Float64RoundDown(x);
  }

  TNode<Float64T> one = Float64Constant(1.0);
  TNode<Float64T> zero = Float64Constant(0.0);
  TNode<Float64T> two_52 = Float64Constant(4503599627370496.0E0);
  TNode<Float64T> minus_two_52 = Float64Constant(-4503599627370496.0E0);

  VARIABLE(var_x, MachineRepresentation::kFloat64, x);
  Label return_x(this), return_minus_x(this);

  // Check if {x} is greater than zero.
  Label if_xgreaterthanzero(this), if_xnotgreaterthanzero(this);
  Branch(Float64GreaterThan(x, zero), &if_xgreaterthanzero,
         &if_xnotgreaterthanzero);

  BIND(&if_xgreaterthanzero);
  {
    // Just return {x} unless it's in the range ]0,2^52[.
    GotoIf(Float64GreaterThanOrEqual(x, two_52), &return_x);

    // Round positive {x} towards -Infinity.
    var_x.Bind(Float64Sub(Float64Add(two_52, x), two_52));
    GotoIfNot(Float64GreaterThan(var_x.value(), x), &return_x);
    var_x.Bind(Float64Sub(var_x.value(), one));
    Goto(&return_x);
  }

  BIND(&if_xnotgreaterthanzero);
  {
    // Just return {x} unless it's in the range ]-2^52,0[
    GotoIf(Float64LessThanOrEqual(x, minus_two_52), &return_x);
    GotoIfNot(Float64LessThan(x, zero), &return_x);

    // Round negated {x} towards -Infinity and return the result negated.
    TNode<Float64T> minus_x = Float64Neg(x);
    var_x.Bind(Float64Sub(Float64Add(two_52, minus_x), two_52));
    GotoIfNot(Float64LessThan(var_x.value(), minus_x), &return_minus_x);
    var_x.Bind(Float64Add(var_x.value(), one));
    Goto(&return_minus_x);
  }

  BIND(&return_minus_x);
  var_x.Bind(Float64Neg(var_x.value()));
  Goto(&return_x);

  BIND(&return_x);
  return TNode<Float64T>::UncheckedCast(var_x.value());
}

TNode<Float64T> CodeStubAssembler::Float64RoundToEven(SloppyTNode<Float64T> x) {
  if (IsFloat64RoundTiesEvenSupported()) {
    return Float64RoundTiesEven(x);
  }
  // See ES#sec-touint8clamp for details.
  TNode<Float64T> f = Float64Floor(x);
  TNode<Float64T> f_and_half = Float64Add(f, Float64Constant(0.5));

  VARIABLE(var_result, MachineRepresentation::kFloat64);
  Label return_f(this), return_f_plus_one(this), done(this);

  GotoIf(Float64LessThan(f_and_half, x), &return_f_plus_one);
  GotoIf(Float64LessThan(x, f_and_half), &return_f);
  {
    TNode<Float64T> f_mod_2 = Float64Mod(f, Float64Constant(2.0));
    Branch(Float64Equal(f_mod_2, Float64Constant(0.0)), &return_f,
           &return_f_plus_one);
  }

  BIND(&return_f);
  var_result.Bind(f);
  Goto(&done);

  BIND(&return_f_plus_one);
  var_result.Bind(Float64Add(f, Float64Constant(1.0)));
  Goto(&done);

  BIND(&done);
  return TNode<Float64T>::UncheckedCast(var_result.value());
}

TNode<Float64T> CodeStubAssembler::Float64Trunc(SloppyTNode<Float64T> x) {
  if (IsFloat64RoundTruncateSupported()) {
    return Float64RoundTruncate(x);
  }

  TNode<Float64T> one = Float64Constant(1.0);
  TNode<Float64T> zero = Float64Constant(0.0);
  TNode<Float64T> two_52 = Float64Constant(4503599627370496.0E0);
  TNode<Float64T> minus_two_52 = Float64Constant(-4503599627370496.0E0);

  VARIABLE(var_x, MachineRepresentation::kFloat64, x);
  Label return_x(this), return_minus_x(this);

  // Check if {x} is greater than 0.
  Label if_xgreaterthanzero(this), if_xnotgreaterthanzero(this);
  Branch(Float64GreaterThan(x, zero), &if_xgreaterthanzero,
         &if_xnotgreaterthanzero);

  BIND(&if_xgreaterthanzero);
  {
    if (IsFloat64RoundDownSupported()) {
      var_x.Bind(Float64RoundDown(x));
    } else {
      // Just return {x} unless it's in the range ]0,2^52[.
      GotoIf(Float64GreaterThanOrEqual(x, two_52), &return_x);

      // Round positive {x} towards -Infinity.
      var_x.Bind(Float64Sub(Float64Add(two_52, x), two_52));
      GotoIfNot(Float64GreaterThan(var_x.value(), x), &return_x);
      var_x.Bind(Float64Sub(var_x.value(), one));
    }
    Goto(&return_x);
  }

  BIND(&if_xnotgreaterthanzero);
  {
    if (IsFloat64RoundUpSupported()) {
      var_x.Bind(Float64RoundUp(x));
      Goto(&return_x);
    } else {
      // Just return {x} unless its in the range ]-2^52,0[.
      GotoIf(Float64LessThanOrEqual(x, minus_two_52), &return_x);
      GotoIfNot(Float64LessThan(x, zero), &return_x);

      // Round negated {x} towards -Infinity and return result negated.
      TNode<Float64T> minus_x = Float64Neg(x);
      var_x.Bind(Float64Sub(Float64Add(two_52, minus_x), two_52));
      GotoIfNot(Float64GreaterThan(var_x.value(), minus_x), &return_minus_x);
      var_x.Bind(Float64Sub(var_x.value(), one));
      Goto(&return_minus_x);
    }
  }

  BIND(&return_minus_x);
  var_x.Bind(Float64Neg(var_x.value()));
  Goto(&return_x);

  BIND(&return_x);
  return TNode<Float64T>::UncheckedCast(var_x.value());
}

TNode<BoolT> CodeStubAssembler::IsValidSmi(TNode<Smi> smi) {
  if (SmiValuesAre32Bits() && kSystemPointerSize == kInt64Size) {
    // Check that the Smi value is zero in the lower bits.
    TNode<IntPtrT> value = BitcastTaggedSignedToWord(smi);
    return Word32Equal(Int32Constant(0), TruncateIntPtrToInt32(value));
  }
  return Int32TrueConstant();
}

Node* CodeStubAssembler::SmiShiftBitsConstant() {
  return IntPtrConstant(kSmiShiftSize + kSmiTagSize);
}

TNode<Smi> CodeStubAssembler::SmiFromInt32(SloppyTNode<Int32T> value) {
  TNode<IntPtrT> value_intptr = ChangeInt32ToIntPtr(value);
  TNode<Smi> smi =
      BitcastWordToTaggedSigned(WordShl(value_intptr, SmiShiftBitsConstant()));
  return smi;
}

TNode<BoolT> CodeStubAssembler::IsValidPositiveSmi(TNode<IntPtrT> value) {
  intptr_t constant_value;
  if (ToIntPtrConstant(value, &constant_value)) {
    return (static_cast<uintptr_t>(constant_value) <=
            static_cast<uintptr_t>(Smi::kMaxValue))
               ? Int32TrueConstant()
               : Int32FalseConstant();
  }

  return UintPtrLessThanOrEqual(value, IntPtrConstant(Smi::kMaxValue));
}

TNode<Smi> CodeStubAssembler::SmiTag(SloppyTNode<IntPtrT> value) {
  int32_t constant_value;
  if (ToInt32Constant(value, &constant_value) && Smi::IsValid(constant_value)) {
    return SmiConstant(constant_value);
  }
  TNode<Smi> smi =
      BitcastWordToTaggedSigned(WordShl(value, SmiShiftBitsConstant()));
  return smi;
}

TNode<IntPtrT> CodeStubAssembler::SmiUntag(SloppyTNode<Smi> value) {
  intptr_t constant_value;
  if (ToIntPtrConstant(value, &constant_value)) {
    return IntPtrConstant(constant_value >> (kSmiShiftSize + kSmiTagSize));
  }
  return Signed(
      WordSar(BitcastTaggedSignedToWord(value), SmiShiftBitsConstant()));
}

TNode<Int32T> CodeStubAssembler::SmiToInt32(SloppyTNode<Smi> value) {
  TNode<IntPtrT> result = SmiUntag(value);
  return TruncateIntPtrToInt32(result);
}

TNode<Float64T> CodeStubAssembler::SmiToFloat64(SloppyTNode<Smi> value) {
  return ChangeInt32ToFloat64(SmiToInt32(value));
}

TNode<Smi> CodeStubAssembler::SmiMax(TNode<Smi> a, TNode<Smi> b) {
  return SelectConstant<Smi>(SmiLessThan(a, b), b, a);
}

TNode<Smi> CodeStubAssembler::SmiMin(TNode<Smi> a, TNode<Smi> b) {
  return SelectConstant<Smi>(SmiLessThan(a, b), a, b);
}

TNode<IntPtrT> CodeStubAssembler::TryIntPtrAdd(TNode<IntPtrT> a,
                                               TNode<IntPtrT> b,
                                               Label* if_overflow) {
  TNode<PairT<IntPtrT, BoolT>> pair = IntPtrAddWithOverflow(a, b);
  TNode<BoolT> overflow = Projection<1>(pair);
  GotoIf(overflow, if_overflow);
  return Projection<0>(pair);
}

TNode<IntPtrT> CodeStubAssembler::TryIntPtrSub(TNode<IntPtrT> a,
                                               TNode<IntPtrT> b,
                                               Label* if_overflow) {
  TNode<PairT<IntPtrT, BoolT>> pair = IntPtrSubWithOverflow(a, b);
  TNode<BoolT> overflow = Projection<1>(pair);
  GotoIf(overflow, if_overflow);
  return Projection<0>(pair);
}

TNode<Int32T> CodeStubAssembler::TryInt32Mul(TNode<Int32T> a, TNode<Int32T> b,
                                             Label* if_overflow) {
  TNode<PairT<Int32T, BoolT>> pair = Int32MulWithOverflow(a, b);
  TNode<BoolT> overflow = Projection<1>(pair);
  GotoIf(overflow, if_overflow);
  return Projection<0>(pair);
}

TNode<Smi> CodeStubAssembler::TrySmiAdd(TNode<Smi> lhs, TNode<Smi> rhs,
                                        Label* if_overflow) {
  if (SmiValuesAre32Bits()) {
    return BitcastWordToTaggedSigned(
        TryIntPtrAdd(BitcastTaggedSignedToWord(lhs),
                     BitcastTaggedSignedToWord(rhs), if_overflow));
  } else {
    DCHECK(SmiValuesAre31Bits());
    TNode<PairT<Int32T, BoolT>> pair = Int32AddWithOverflow(
        TruncateIntPtrToInt32(BitcastTaggedSignedToWord(lhs)),
        TruncateIntPtrToInt32(BitcastTaggedSignedToWord(rhs)));
    TNode<BoolT> overflow = Projection<1>(pair);
    GotoIf(overflow, if_overflow);
    TNode<Int32T> result = Projection<0>(pair);
    return BitcastWordToTaggedSigned(ChangeInt32ToIntPtr(result));
  }
}

TNode<Smi> CodeStubAssembler::TrySmiSub(TNode<Smi> lhs, TNode<Smi> rhs,
                                        Label* if_overflow) {
  if (SmiValuesAre32Bits()) {
    TNode<PairT<IntPtrT, BoolT>> pair = IntPtrSubWithOverflow(
        BitcastTaggedSignedToWord(lhs), BitcastTaggedSignedToWord(rhs));
    TNode<BoolT> overflow = Projection<1>(pair);
    GotoIf(overflow, if_overflow);
    TNode<IntPtrT> result = Projection<0>(pair);
    return BitcastWordToTaggedSigned(result);
  } else {
    DCHECK(SmiValuesAre31Bits());
    TNode<PairT<Int32T, BoolT>> pair = Int32SubWithOverflow(
        TruncateIntPtrToInt32(BitcastTaggedSignedToWord(lhs)),
        TruncateIntPtrToInt32(BitcastTaggedSignedToWord(rhs)));
    TNode<BoolT> overflow = Projection<1>(pair);
    GotoIf(overflow, if_overflow);
    TNode<Int32T> result = Projection<0>(pair);
    return BitcastWordToTaggedSigned(ChangeInt32ToIntPtr(result));
  }
}

TNode<Number> CodeStubAssembler::NumberMax(SloppyTNode<Number> a,
                                           SloppyTNode<Number> b) {
  // TODO(danno): This could be optimized by specifically handling smi cases.
  TVARIABLE(Number, result);
  Label done(this), greater_than_equal_a(this), greater_than_equal_b(this);
  GotoIfNumberGreaterThanOrEqual(a, b, &greater_than_equal_a);
  GotoIfNumberGreaterThanOrEqual(b, a, &greater_than_equal_b);
  result = NanConstant();
  Goto(&done);
  BIND(&greater_than_equal_a);
  result = a;
  Goto(&done);
  BIND(&greater_than_equal_b);
  result = b;
  Goto(&done);
  BIND(&done);
  return result.value();
}

TNode<Number> CodeStubAssembler::NumberMin(SloppyTNode<Number> a,
                                           SloppyTNode<Number> b) {
  // TODO(danno): This could be optimized by specifically handling smi cases.
  TVARIABLE(Number, result);
  Label done(this), greater_than_equal_a(this), greater_than_equal_b(this);
  GotoIfNumberGreaterThanOrEqual(a, b, &greater_than_equal_a);
  GotoIfNumberGreaterThanOrEqual(b, a, &greater_than_equal_b);
  result = NanConstant();
  Goto(&done);
  BIND(&greater_than_equal_a);
  result = b;
  Goto(&done);
  BIND(&greater_than_equal_b);
  result = a;
  Goto(&done);
  BIND(&done);
  return result.value();
}

TNode<IntPtrT> CodeStubAssembler::ConvertToRelativeIndex(
    TNode<Context> context, TNode<Object> index, TNode<IntPtrT> length) {
  TVARIABLE(IntPtrT, result);

  TNode<Number> const index_int =
      ToInteger_Inline(context, index, CodeStubAssembler::kTruncateMinusZero);
  TNode<IntPtrT> zero = IntPtrConstant(0);

  Label done(this);
  Label if_issmi(this), if_isheapnumber(this, Label::kDeferred);
  Branch(TaggedIsSmi(index_int), &if_issmi, &if_isheapnumber);

  BIND(&if_issmi);
  {
    TNode<Smi> const index_smi = CAST(index_int);
    result = Select<IntPtrT>(
        IntPtrLessThan(SmiUntag(index_smi), zero),
        [=] { return IntPtrMax(IntPtrAdd(length, SmiUntag(index_smi)), zero); },
        [=] { return IntPtrMin(SmiUntag(index_smi), length); });
    Goto(&done);
  }

  BIND(&if_isheapnumber);
  {
    // If {index} is a heap number, it is definitely out of bounds. If it is
    // negative, {index} = max({length} + {index}),0) = 0'. If it is positive,
    // set {index} to {length}.
    TNode<HeapNumber> const index_hn = CAST(index_int);
    TNode<Float64T> const float_zero = Float64Constant(0.);
    TNode<Float64T> const index_float = LoadHeapNumberValue(index_hn);
    result = SelectConstant<IntPtrT>(Float64LessThan(index_float, float_zero),
                                     zero, length);
    Goto(&done);
  }
  BIND(&done);
  return result.value();
}

TNode<Number> CodeStubAssembler::SmiMod(TNode<Smi> a, TNode<Smi> b) {
  TVARIABLE(Number, var_result);
  Label return_result(this, &var_result),
      return_minuszero(this, Label::kDeferred),
      return_nan(this, Label::kDeferred);

  // Untag {a} and {b}.
  TNode<Int32T> int_a = SmiToInt32(a);
  TNode<Int32T> int_b = SmiToInt32(b);

  // Return NaN if {b} is zero.
  GotoIf(Word32Equal(int_b, Int32Constant(0)), &return_nan);

  // Check if {a} is non-negative.
  Label if_aisnotnegative(this), if_aisnegative(this, Label::kDeferred);
  Branch(Int32LessThanOrEqual(Int32Constant(0), int_a), &if_aisnotnegative,
         &if_aisnegative);

  BIND(&if_aisnotnegative);
  {
    // Fast case, don't need to check any other edge cases.
    TNode<Int32T> r = Int32Mod(int_a, int_b);
    var_result = SmiFromInt32(r);
    Goto(&return_result);
  }

  BIND(&if_aisnegative);
  {
    if (SmiValuesAre32Bits()) {
      // Check if {a} is kMinInt and {b} is -1 (only relevant if the
      // kMinInt is actually representable as a Smi).
      Label join(this);
      GotoIfNot(Word32Equal(int_a, Int32Constant(kMinInt)), &join);
      GotoIf(Word32Equal(int_b, Int32Constant(-1)), &return_minuszero);
      Goto(&join);
      BIND(&join);
    }

    // Perform the integer modulus operation.
    TNode<Int32T> r = Int32Mod(int_a, int_b);

    // Check if {r} is zero, and if so return -0, because we have to
    // take the sign of the left hand side {a}, which is negative.
    GotoIf(Word32Equal(r, Int32Constant(0)), &return_minuszero);

    // The remainder {r} can be outside the valid Smi range on 32bit
    // architectures, so we cannot just say SmiFromInt32(r) here.
    var_result = ChangeInt32ToTagged(r);
    Goto(&return_result);
  }

  BIND(&return_minuszero);
  var_result = MinusZeroConstant();
  Goto(&return_result);

  BIND(&return_nan);
  var_result = NanConstant();
  Goto(&return_result);

  BIND(&return_result);
  return var_result.value();
}

TNode<Number> CodeStubAssembler::SmiMul(TNode<Smi> a, TNode<Smi> b) {
  TVARIABLE(Number, var_result);
  VARIABLE(var_lhs_float64, MachineRepresentation::kFloat64);
  VARIABLE(var_rhs_float64, MachineRepresentation::kFloat64);
  Label return_result(this, &var_result);

  // Both {a} and {b} are Smis. Convert them to integers and multiply.
  TNode<Int32T> lhs32 = SmiToInt32(a);
  TNode<Int32T> rhs32 = SmiToInt32(b);
  auto pair = Int32MulWithOverflow(lhs32, rhs32);

  TNode<BoolT> overflow = Projection<1>(pair);

  // Check if the multiplication overflowed.
  Label if_overflow(this, Label::kDeferred), if_notoverflow(this);
  Branch(overflow, &if_overflow, &if_notoverflow);
  BIND(&if_notoverflow);
  {
    // If the answer is zero, we may need to return -0.0, depending on the
    // input.
    Label answer_zero(this), answer_not_zero(this);
    TNode<Int32T> answer = Projection<0>(pair);
    TNode<Int32T> zero = Int32Constant(0);
    Branch(Word32Equal(answer, zero), &answer_zero, &answer_not_zero);
    BIND(&answer_not_zero);
    {
      var_result = ChangeInt32ToTagged(answer);
      Goto(&return_result);
    }
    BIND(&answer_zero);
    {
      TNode<Word32T> or_result = Word32Or(lhs32, rhs32);
      Label if_should_be_negative_zero(this), if_should_be_zero(this);
      Branch(Int32LessThan(or_result, zero), &if_should_be_negative_zero,
             &if_should_be_zero);
      BIND(&if_should_be_negative_zero);
      {
        var_result = MinusZeroConstant();
        Goto(&return_result);
      }
      BIND(&if_should_be_zero);
      {
        var_result = SmiConstant(0);
        Goto(&return_result);
      }
    }
  }
  BIND(&if_overflow);
  {
    var_lhs_float64.Bind(SmiToFloat64(a));
    var_rhs_float64.Bind(SmiToFloat64(b));
    TNode<Float64T> value =
        Float64Mul(var_lhs_float64.value(), var_rhs_float64.value());
    var_result = AllocateHeapNumberWithValue(value);
    Goto(&return_result);
  }

  BIND(&return_result);
  return var_result.value();
}

TNode<Smi> CodeStubAssembler::TrySmiDiv(TNode<Smi> dividend, TNode<Smi> divisor,
                                        Label* bailout) {
  // Both {a} and {b} are Smis. Bailout to floating point division if {divisor}
  // is zero.
  GotoIf(TaggedEqual(divisor, SmiConstant(0)), bailout);

  // Do floating point division if {dividend} is zero and {divisor} is
  // negative.
  Label dividend_is_zero(this), dividend_is_not_zero(this);
  Branch(TaggedEqual(dividend, SmiConstant(0)), &dividend_is_zero,
         &dividend_is_not_zero);

  BIND(&dividend_is_zero);
  {
    GotoIf(SmiLessThan(divisor, SmiConstant(0)), bailout);
    Goto(&dividend_is_not_zero);
  }
  BIND(&dividend_is_not_zero);

  TNode<Int32T> untagged_divisor = SmiToInt32(divisor);
  TNode<Int32T> untagged_dividend = SmiToInt32(dividend);

  // Do floating point division if {dividend} is kMinInt (or kMinInt - 1
  // if the Smi size is 31) and {divisor} is -1.
  Label divisor_is_minus_one(this), divisor_is_not_minus_one(this);
  Branch(Word32Equal(untagged_divisor, Int32Constant(-1)),
         &divisor_is_minus_one, &divisor_is_not_minus_one);

  BIND(&divisor_is_minus_one);
  {
    GotoIf(Word32Equal(
               untagged_dividend,
               Int32Constant(kSmiValueSize == 32 ? kMinInt : (kMinInt >> 1))),
           bailout);
    Goto(&divisor_is_not_minus_one);
  }
  BIND(&divisor_is_not_minus_one);

  TNode<Int32T> untagged_result = Int32Div(untagged_dividend, untagged_divisor);
  TNode<Int32T> truncated = Int32Mul(untagged_result, untagged_divisor);

  // Do floating point division if the remainder is not 0.
  GotoIf(Word32NotEqual(untagged_dividend, truncated), bailout);

  return SmiFromInt32(untagged_result);
}

TNode<Smi> CodeStubAssembler::SmiLexicographicCompare(TNode<Smi> x,
                                                      TNode<Smi> y) {
  TNode<ExternalReference> smi_lexicographic_compare =
      ExternalConstant(ExternalReference::smi_lexicographic_compare_function());
  TNode<ExternalReference> isolate_ptr =
      ExternalConstant(ExternalReference::isolate_address(isolate()));
  return CAST(CallCFunction(smi_lexicographic_compare, MachineType::AnyTagged(),
                            std::make_pair(MachineType::Pointer(), isolate_ptr),
                            std::make_pair(MachineType::AnyTagged(), x),
                            std::make_pair(MachineType::AnyTagged(), y)));
}

TNode<Int32T> CodeStubAssembler::TruncateWordToInt32(SloppyTNode<WordT> value) {
  if (Is64()) {
    return TruncateInt64ToInt32(ReinterpretCast<Int64T>(value));
  }
  return ReinterpretCast<Int32T>(value);
}

TNode<Int32T> CodeStubAssembler::TruncateIntPtrToInt32(
    SloppyTNode<IntPtrT> value) {
  if (Is64()) {
    return TruncateInt64ToInt32(ReinterpretCast<Int64T>(value));
  }
  return ReinterpretCast<Int32T>(value);
}

TNode<BoolT> CodeStubAssembler::TaggedIsSmi(SloppyTNode<Object> a) {
  STATIC_ASSERT(kSmiTagMask < kMaxUInt32);
  return Word32Equal(Word32And(TruncateIntPtrToInt32(BitcastTaggedToWord(a)),
                               Int32Constant(kSmiTagMask)),
                     Int32Constant(0));
}

TNode<BoolT> CodeStubAssembler::TaggedIsSmi(TNode<MaybeObject> a) {
  STATIC_ASSERT(kSmiTagMask < kMaxUInt32);
  return Word32Equal(
      Word32And(TruncateIntPtrToInt32(BitcastMaybeObjectToWord(a)),
                Int32Constant(kSmiTagMask)),
      Int32Constant(0));
}

TNode<BoolT> CodeStubAssembler::TaggedIsNotSmi(SloppyTNode<Object> a) {
  // Although BitcastTaggedSignedToWord is generally unsafe on HeapObjects, we
  // can nonetheless use it to inspect the Smi tag. The assumption here is that
  // the GC will not exchange Smis for HeapObjects or vice-versa.
  TNode<IntPtrT> a_bitcast = BitcastTaggedSignedToWord(UncheckedCast<Smi>(a));
  STATIC_ASSERT(kSmiTagMask < kMaxUInt32);
  return Word32NotEqual(
      Word32And(TruncateIntPtrToInt32(a_bitcast), Int32Constant(kSmiTagMask)),
      Int32Constant(0));
}

TNode<BoolT> CodeStubAssembler::TaggedIsPositiveSmi(SloppyTNode<Object> a) {
#if defined(V8_HOST_ARCH_32_BIT) || defined(V8_31BIT_SMIS_ON_64BIT_ARCH)
  return Word32Equal(
      Word32And(
          TruncateIntPtrToInt32(BitcastTaggedToWord(a)),
          Uint32Constant(kSmiTagMask | static_cast<int32_t>(kSmiSignMask))),
      Int32Constant(0));
#else
  return WordEqual(WordAnd(BitcastTaggedToWord(a),
                           IntPtrConstant(kSmiTagMask | kSmiSignMask)),
                   IntPtrConstant(0));
#endif
}

TNode<BoolT> CodeStubAssembler::WordIsAligned(SloppyTNode<WordT> word,
                                              size_t alignment) {
  DCHECK(base::bits::IsPowerOfTwo(alignment));
  DCHECK_LE(alignment, kMaxUInt32);
  return Word32Equal(
      Int32Constant(0),
      Word32And(TruncateWordToInt32(word),
                Uint32Constant(static_cast<uint32_t>(alignment) - 1)));
}

#if DEBUG
void CodeStubAssembler::Bind(Label* label, AssemblerDebugInfo debug_info) {
  CodeAssembler::Bind(label, debug_info);
}
#endif  // DEBUG

void CodeStubAssembler::Bind(Label* label) { CodeAssembler::Bind(label); }

TNode<Float64T> CodeStubAssembler::LoadDoubleWithHoleCheck(
    TNode<FixedDoubleArray> array, TNode<Smi> index, Label* if_hole) {
  return LoadFixedDoubleArrayElement(array, index, MachineType::Float64(), 0,
                                     SMI_PARAMETERS, if_hole);
}

TNode<Float64T> CodeStubAssembler::LoadDoubleWithHoleCheck(
    TNode<FixedDoubleArray> array, TNode<IntPtrT> index, Label* if_hole) {
  return LoadFixedDoubleArrayElement(array, index, MachineType::Float64(), 0,
                                     INTPTR_PARAMETERS, if_hole);
}

void CodeStubAssembler::BranchIfPrototypesHaveNoElements(
    Node* receiver_map, Label* definitely_no_elements,
    Label* possibly_elements) {
  CSA_SLOW_ASSERT(this, IsMap(receiver_map));
  VARIABLE(var_map, MachineRepresentation::kTagged, receiver_map);
  Label loop_body(this, &var_map);
  TNode<FixedArray> empty_fixed_array = EmptyFixedArrayConstant();
  TNode<NumberDictionary> empty_slow_element_dictionary =
      EmptySlowElementDictionaryConstant();
  Goto(&loop_body);

  BIND(&loop_body);
  {
    Node* map = var_map.value();
    TNode<HeapObject> prototype = LoadMapPrototype(map);
    GotoIf(IsNull(prototype), definitely_no_elements);
    TNode<Map> prototype_map = LoadMap(prototype);
    TNode<Uint16T> prototype_instance_type = LoadMapInstanceType(prototype_map);

    // Pessimistically assume elements if a Proxy, Special API Object,
    // or JSPrimitiveWrapper wrapper is found on the prototype chain. After this
    // instance type check, it's not necessary to check for interceptors or
    // access checks.
    Label if_custom(this, Label::kDeferred), if_notcustom(this);
    Branch(IsCustomElementsReceiverInstanceType(prototype_instance_type),
           &if_custom, &if_notcustom);

    BIND(&if_custom);
    {
      // For string JSPrimitiveWrapper wrappers we still support the checks as
      // long as they wrap the empty string.
      GotoIfNot(
          InstanceTypeEqual(prototype_instance_type, JS_PRIMITIVE_WRAPPER_TYPE),
          possibly_elements);
      Node* prototype_value = LoadJSPrimitiveWrapperValue(prototype);
      Branch(IsEmptyString(prototype_value), &if_notcustom, possibly_elements);
    }

    BIND(&if_notcustom);
    {
      TNode<FixedArrayBase> prototype_elements = LoadElements(CAST(prototype));
      var_map.Bind(prototype_map);
      GotoIf(TaggedEqual(prototype_elements, empty_fixed_array), &loop_body);
      Branch(TaggedEqual(prototype_elements, empty_slow_element_dictionary),
             &loop_body, possibly_elements);
    }
  }
}

void CodeStubAssembler::BranchIfJSReceiver(SloppyTNode<Object> object,
                                           Label* if_true, Label* if_false) {
  GotoIf(TaggedIsSmi(object), if_false);
  STATIC_ASSERT(LAST_JS_RECEIVER_TYPE == LAST_TYPE);
  Branch(IsJSReceiver(CAST(object)), if_true, if_false);
}

void CodeStubAssembler::GotoIfForceSlowPath(Label* if_true) {
#ifdef V8_ENABLE_FORCE_SLOW_PATH
  TNode<ExternalReference> const force_slow_path_addr =
      ExternalConstant(ExternalReference::force_slow_path(isolate()));
  Node* const force_slow = Load(MachineType::Uint8(), force_slow_path_addr);

  GotoIf(force_slow, if_true);
#endif
}

void CodeStubAssembler::GotoIfDebugExecutionModeChecksSideEffects(
    Label* if_true) {
  STATIC_ASSERT(sizeof(DebugInfo::ExecutionMode) >= sizeof(int32_t));

  TNode<ExternalReference> execution_mode_address = ExternalConstant(
      ExternalReference::debug_execution_mode_address(isolate()));
  TNode<Int32T> execution_mode =
      UncheckedCast<Int32T>(Load(MachineType::Int32(), execution_mode_address));

  GotoIf(Word32Equal(execution_mode, Int32Constant(DebugInfo::kSideEffects)),
         if_true);
}

TNode<HeapObject> CodeStubAssembler::AllocateRaw(TNode<IntPtrT> size_in_bytes,
                                                 AllocationFlags flags,
                                                 TNode<RawPtrT> top_address,
                                                 TNode<RawPtrT> limit_address) {
  Label if_out_of_memory(this, Label::kDeferred);

  // TODO(jgruber,jkummerow): Extract the slow paths (= probably everything
  // but bump pointer allocation) into a builtin to save code space. The
  // size_in_bytes check may be moved there as well since a non-smi
  // size_in_bytes probably doesn't fit into the bump pointer region
  // (double-check that).

  intptr_t size_in_bytes_constant;
  bool size_in_bytes_is_constant = false;
  if (ToIntPtrConstant(size_in_bytes, &size_in_bytes_constant)) {
    size_in_bytes_is_constant = true;
    CHECK(Internals::IsValidSmi(size_in_bytes_constant));
    CHECK_GT(size_in_bytes_constant, 0);
  } else {
    GotoIfNot(IsValidPositiveSmi(size_in_bytes), &if_out_of_memory);
  }

  TNode<RawPtrT> top =
      UncheckedCast<RawPtrT>(Load(MachineType::Pointer(), top_address));
  TNode<RawPtrT> limit =
      UncheckedCast<RawPtrT>(Load(MachineType::Pointer(), limit_address));

  // If there's not enough space, call the runtime.
  TVARIABLE(Object, result);
  Label runtime_call(this, Label::kDeferred), no_runtime_call(this), out(this);

  bool needs_double_alignment = flags & kDoubleAlignment;
  bool allow_large_object_allocation = flags & kAllowLargeObjectAllocation;

  if (allow_large_object_allocation) {
    Label next(this);
    GotoIf(IsRegularHeapObjectSize(size_in_bytes), &next);

    TNode<Smi> runtime_flags = SmiConstant(Smi::FromInt(
        AllocateDoubleAlignFlag::encode(needs_double_alignment) |
        AllowLargeObjectAllocationFlag::encode(allow_large_object_allocation)));
    if (FLAG_young_generation_large_objects) {
      result =
          CallRuntime(Runtime::kAllocateInYoungGeneration, NoContextConstant(),
                      SmiTag(size_in_bytes), runtime_flags);
    } else {
      result =
          CallRuntime(Runtime::kAllocateInOldGeneration, NoContextConstant(),
                      SmiTag(size_in_bytes), runtime_flags);
    }
    Goto(&out);

    BIND(&next);
  }

  TVARIABLE(IntPtrT, adjusted_size, size_in_bytes);

  if (needs_double_alignment) {
    Label next(this);
    GotoIfNot(WordAnd(top, IntPtrConstant(kDoubleAlignmentMask)), &next);

    adjusted_size = IntPtrAdd(size_in_bytes, IntPtrConstant(4));
    Goto(&next);

    BIND(&next);
  }

  TNode<IntPtrT> new_top =
      IntPtrAdd(UncheckedCast<IntPtrT>(top), adjusted_size.value());

  Branch(UintPtrGreaterThanOrEqual(new_top, limit), &runtime_call,
         &no_runtime_call);

  BIND(&runtime_call);
  {
    TNode<Smi> runtime_flags = SmiConstant(Smi::FromInt(
        AllocateDoubleAlignFlag::encode(needs_double_alignment) |
        AllowLargeObjectAllocationFlag::encode(allow_large_object_allocation)));
    if (flags & kPretenured) {
      result =
          CallRuntime(Runtime::kAllocateInOldGeneration, NoContextConstant(),
                      SmiTag(size_in_bytes), runtime_flags);
    } else {
      result =
          CallRuntime(Runtime::kAllocateInYoungGeneration, NoContextConstant(),
                      SmiTag(size_in_bytes), runtime_flags);
    }
    Goto(&out);
  }

  // When there is enough space, return `top' and bump it up.
  BIND(&no_runtime_call);
  {
    StoreNoWriteBarrier(MachineType::PointerRepresentation(), top_address,
                        new_top);

    TVARIABLE(IntPtrT, address, UncheckedCast<IntPtrT>(top));

    if (needs_double_alignment) {
      Label next(this);
      GotoIf(IntPtrEqual(adjusted_size.value(), size_in_bytes), &next);

      // Store a filler and increase the address by 4.
      StoreNoWriteBarrier(MachineRepresentation::kTagged, top,
                          OnePointerFillerMapConstant());
      address = IntPtrAdd(UncheckedCast<IntPtrT>(top), IntPtrConstant(4));
      Goto(&next);

      BIND(&next);
    }

    result = BitcastWordToTagged(
        IntPtrAdd(address.value(), IntPtrConstant(kHeapObjectTag)));
    Goto(&out);
  }

  if (!size_in_bytes_is_constant) {
    BIND(&if_out_of_memory);
    CallRuntime(Runtime::kFatalProcessOutOfMemoryInAllocateRaw,
                NoContextConstant());
    Unreachable();
  }

  BIND(&out);
  return UncheckedCast<HeapObject>(result.value());
}

TNode<HeapObject> CodeStubAssembler::AllocateRawUnaligned(
    TNode<IntPtrT> size_in_bytes, AllocationFlags flags,
    TNode<RawPtrT> top_address, TNode<RawPtrT> limit_address) {
  DCHECK_EQ(flags & kDoubleAlignment, 0);
  return AllocateRaw(size_in_bytes, flags, top_address, limit_address);
}

TNode<HeapObject> CodeStubAssembler::AllocateRawDoubleAligned(
    TNode<IntPtrT> size_in_bytes, AllocationFlags flags,
    TNode<RawPtrT> top_address, TNode<RawPtrT> limit_address) {
#if defined(V8_HOST_ARCH_32_BIT)
  return AllocateRaw(size_in_bytes, flags | kDoubleAlignment, top_address,
                     limit_address);
#elif defined(V8_HOST_ARCH_64_BIT)
#ifdef V8_COMPRESS_POINTERS
  // TODO(ishell, v8:8875): Consider using aligned allocations once the
  // allocation alignment inconsistency is fixed. For now we keep using
  // unaligned access since both x64 and arm64 architectures (where pointer
  // compression is supported) allow unaligned access to doubles and full words.
#endif  // V8_COMPRESS_POINTERS
  // Allocation on 64 bit machine is naturally double aligned
  return AllocateRaw(size_in_bytes, flags & ~kDoubleAlignment, top_address,
                     limit_address);
#else
#error Architecture not supported
#endif
}

TNode<HeapObject> CodeStubAssembler::AllocateInNewSpace(
    TNode<IntPtrT> size_in_bytes, AllocationFlags flags) {
  DCHECK(flags == kNone || flags == kDoubleAlignment);
  CSA_ASSERT(this, IsRegularHeapObjectSize(size_in_bytes));
  return Allocate(size_in_bytes, flags);
}

TNode<HeapObject> CodeStubAssembler::Allocate(TNode<IntPtrT> size_in_bytes,
                                              AllocationFlags flags) {
  Comment("Allocate");
  bool const new_space = !(flags & kPretenured);
  bool const allow_large_objects = flags & kAllowLargeObjectAllocation;
  // For optimized allocations, we don't allow the allocation to happen in a
  // different generation than requested.
  bool const always_allocated_in_requested_space =
      !new_space || !allow_large_objects || FLAG_young_generation_large_objects;
  if (!allow_large_objects) {
    intptr_t size_constant;
    if (ToIntPtrConstant(size_in_bytes, &size_constant)) {
      CHECK_LE(size_constant, kMaxRegularHeapObjectSize);
    } else {
      CSA_ASSERT(this, IsRegularHeapObjectSize(size_in_bytes));
    }
  }
  if (!(flags & kDoubleAlignment) && always_allocated_in_requested_space) {
    return OptimizedAllocate(
        size_in_bytes,
        new_space ? AllocationType::kYoung : AllocationType::kOld,
        allow_large_objects ? AllowLargeObjects::kTrue
                            : AllowLargeObjects::kFalse);
  }
  TNode<ExternalReference> top_address = ExternalConstant(
      new_space
          ? ExternalReference::new_space_allocation_top_address(isolate())
          : ExternalReference::old_space_allocation_top_address(isolate()));
  DCHECK_EQ(kSystemPointerSize,
            ExternalReference::new_space_allocation_limit_address(isolate())
                    .address() -
                ExternalReference::new_space_allocation_top_address(isolate())
                    .address());
  DCHECK_EQ(kSystemPointerSize,
            ExternalReference::old_space_allocation_limit_address(isolate())
                    .address() -
                ExternalReference::old_space_allocation_top_address(isolate())
                    .address());
  TNode<IntPtrT> limit_address =
      IntPtrAdd(ReinterpretCast<IntPtrT>(top_address),
                IntPtrConstant(kSystemPointerSize));

  if (flags & kDoubleAlignment) {
    return AllocateRawDoubleAligned(size_in_bytes, flags,
                                    ReinterpretCast<RawPtrT>(top_address),
                                    ReinterpretCast<RawPtrT>(limit_address));
  } else {
    return AllocateRawUnaligned(size_in_bytes, flags,
                                ReinterpretCast<RawPtrT>(top_address),
                                ReinterpretCast<RawPtrT>(limit_address));
  }
}

TNode<HeapObject> CodeStubAssembler::AllocateInNewSpace(int size_in_bytes,
                                                        AllocationFlags flags) {
  CHECK(flags == kNone || flags == kDoubleAlignment);
  DCHECK_LE(size_in_bytes, kMaxRegularHeapObjectSize);
  return CodeStubAssembler::Allocate(IntPtrConstant(size_in_bytes), flags);
}

TNode<HeapObject> CodeStubAssembler::Allocate(int size_in_bytes,
                                              AllocationFlags flags) {
  return CodeStubAssembler::Allocate(IntPtrConstant(size_in_bytes), flags);
}

TNode<HeapObject> CodeStubAssembler::InnerAllocate(TNode<HeapObject> previous,
                                                   TNode<IntPtrT> offset) {
  return UncheckedCast<HeapObject>(
      BitcastWordToTagged(IntPtrAdd(BitcastTaggedToWord(previous), offset)));
}

TNode<HeapObject> CodeStubAssembler::InnerAllocate(TNode<HeapObject> previous,
                                                   int offset) {
  return InnerAllocate(previous, IntPtrConstant(offset));
}

TNode<BoolT> CodeStubAssembler::IsRegularHeapObjectSize(TNode<IntPtrT> size) {
  return UintPtrLessThanOrEqual(size,
                                IntPtrConstant(kMaxRegularHeapObjectSize));
}

void CodeStubAssembler::BranchIfToBooleanIsTrue(SloppyTNode<Object> value,
                                                Label* if_true,
                                                Label* if_false) {
  Label if_smi(this), if_notsmi(this), if_heapnumber(this, Label::kDeferred),
      if_bigint(this, Label::kDeferred);
  // Rule out false {value}.
  GotoIf(TaggedEqual(value, FalseConstant()), if_false);

  // Check if {value} is a Smi or a HeapObject.
  Branch(TaggedIsSmi(value), &if_smi, &if_notsmi);

  BIND(&if_smi);
  {
    // The {value} is a Smi, only need to check against zero.
    BranchIfSmiEqual(CAST(value), SmiConstant(0), if_false, if_true);
  }

  BIND(&if_notsmi);
  {
    TNode<HeapObject> value_heapobject = CAST(value);

    // Check if {value} is the empty string.
    GotoIf(IsEmptyString(value_heapobject), if_false);

    // The {value} is a HeapObject, load its map.
    TNode<Map> value_map = LoadMap(value_heapobject);

    // Only null, undefined and document.all have the undetectable bit set,
    // so we can return false immediately when that bit is set.
    GotoIf(IsUndetectableMap(value_map), if_false);

    // We still need to handle numbers specially, but all other {value}s
    // that make it here yield true.
    GotoIf(IsHeapNumberMap(value_map), &if_heapnumber);
    Branch(IsBigInt(value_heapobject), &if_bigint, if_true);

    BIND(&if_heapnumber);
    {
      // Load the floating point value of {value}.
      Node* value_value = LoadObjectField(
          value_heapobject, HeapNumber::kValueOffset, MachineType::Float64());

      // Check if the floating point {value} is neither 0.0, -0.0 nor NaN.
      Branch(Float64LessThan(Float64Constant(0.0), Float64Abs(value_value)),
             if_true, if_false);
    }

    BIND(&if_bigint);
    {
      TNode<BigInt> bigint = CAST(value);
      TNode<Word32T> bitfield = LoadBigIntBitfield(bigint);
      TNode<Uint32T> length = DecodeWord32<BigIntBase::LengthBits>(bitfield);
      Branch(Word32Equal(length, Int32Constant(0)), if_false, if_true);
    }
  }
}

Node* CodeStubAssembler::LoadFromParentFrame(int offset, MachineType type) {
  TNode<RawPtrT> frame_pointer = LoadParentFramePointer();
  return Load(type, frame_pointer, IntPtrConstant(offset));
}

Node* CodeStubAssembler::LoadBufferObject(Node* buffer, int offset,
                                          MachineType type) {
  return Load(type, buffer, IntPtrConstant(offset));
}

Node* CodeStubAssembler::LoadObjectField(SloppyTNode<HeapObject> object,
                                         int offset, MachineType type) {
  CSA_ASSERT(this, IsStrong(object));
  return LoadFromObject(type, object, IntPtrConstant(offset - kHeapObjectTag));
}

Node* CodeStubAssembler::LoadObjectField(SloppyTNode<HeapObject> object,
                                         SloppyTNode<IntPtrT> offset,
                                         MachineType type) {
  CSA_ASSERT(this, IsStrong(object));
  return LoadFromObject(type, object,
                        IntPtrSub(offset, IntPtrConstant(kHeapObjectTag)));
}

TNode<IntPtrT> CodeStubAssembler::LoadAndUntagObjectField(
    SloppyTNode<HeapObject> object, int offset) {
  if (SmiValuesAre32Bits()) {
#if V8_TARGET_LITTLE_ENDIAN
    offset += 4;
#endif
    return ChangeInt32ToIntPtr(
        LoadObjectField(object, offset, MachineType::Int32()));
  } else {
    return SmiToIntPtr(
        LoadObjectField(object, offset, MachineType::TaggedSigned()));
  }
}

TNode<Int32T> CodeStubAssembler::LoadAndUntagToWord32ObjectField(
    SloppyTNode<HeapObject> object, int offset) {
  if (SmiValuesAre32Bits()) {
#if V8_TARGET_LITTLE_ENDIAN
    offset += 4;
#endif
    return UncheckedCast<Int32T>(
        LoadObjectField(object, offset, MachineType::Int32()));
  } else {
    return SmiToInt32(
        LoadObjectField(object, offset, MachineType::TaggedSigned()));
  }
}

TNode<Float64T> CodeStubAssembler::LoadHeapNumberValue(
    SloppyTNode<HeapObject> object) {
  CSA_ASSERT(this, Word32Or(IsHeapNumber(object), IsOddball(object)));
  STATIC_ASSERT(HeapNumber::kValueOffset == Oddball::kToNumberRawOffset);
  return TNode<Float64T>::UncheckedCast(LoadObjectField(
      object, HeapNumber::kValueOffset, MachineType::Float64()));
}

TNode<Map> CodeStubAssembler::GetStructMap(InstanceType instance_type) {
  Handle<Map> map_handle(Map::GetStructMap(isolate(), instance_type),
                         isolate());
  return HeapConstant(map_handle);
}

TNode<Map> CodeStubAssembler::LoadMap(SloppyTNode<HeapObject> object) {
  // TODO(v8:9637): Do a proper LoadObjectField<Map> and remove UncheckedCast
  // when we can avoid making Large code objects due to TNodification.
  return UncheckedCast<Map>(LoadObjectField(object, HeapObject::kMapOffset,
                                            MachineType::TaggedPointer()));
}

TNode<Uint16T> CodeStubAssembler::LoadInstanceType(
    SloppyTNode<HeapObject> object) {
  return LoadMapInstanceType(LoadMap(object));
}

TNode<BoolT> CodeStubAssembler::HasInstanceType(SloppyTNode<HeapObject> object,
                                                InstanceType instance_type) {
  return InstanceTypeEqual(LoadInstanceType(object), instance_type);
}

TNode<BoolT> CodeStubAssembler::DoesntHaveInstanceType(
    SloppyTNode<HeapObject> object, InstanceType instance_type) {
  return Word32NotEqual(LoadInstanceType(object), Int32Constant(instance_type));
}

TNode<BoolT> CodeStubAssembler::TaggedDoesntHaveInstanceType(
    SloppyTNode<HeapObject> any_tagged, InstanceType type) {
  /* return Phi <TaggedIsSmi(val), DoesntHaveInstanceType(val, type)> */
  TNode<BoolT> tagged_is_smi = TaggedIsSmi(any_tagged);
  return Select<BoolT>(
      tagged_is_smi, [=]() { return tagged_is_smi; },
      [=]() { return DoesntHaveInstanceType(any_tagged, type); });
}

TNode<BoolT> CodeStubAssembler::IsSpecialReceiverMap(SloppyTNode<Map> map) {
  CSA_SLOW_ASSERT(this, IsMap(map));
  TNode<BoolT> is_special =
      IsSpecialReceiverInstanceType(LoadMapInstanceType(map));
  uint32_t mask =
      Map::HasNamedInterceptorBit::kMask | Map::IsAccessCheckNeededBit::kMask;
  USE(mask);
  // Interceptors or access checks imply special receiver.
  CSA_ASSERT(this,
             SelectConstant<BoolT>(IsSetWord32(LoadMapBitField(map), mask),
                                   is_special, Int32TrueConstant()));
  return is_special;
}

TNode<Word32T> CodeStubAssembler::IsStringWrapperElementsKind(TNode<Map> map) {
  TNode<Int32T> kind = LoadMapElementsKind(map);
  return Word32Or(
      Word32Equal(kind, Int32Constant(FAST_STRING_WRAPPER_ELEMENTS)),
      Word32Equal(kind, Int32Constant(SLOW_STRING_WRAPPER_ELEMENTS)));
}

void CodeStubAssembler::GotoIfMapHasSlowProperties(TNode<Map> map,
                                                   Label* if_slow) {
  GotoIf(IsStringWrapperElementsKind(map), if_slow);
  GotoIf(IsSpecialReceiverMap(map), if_slow);
  GotoIf(IsDictionaryMap(map), if_slow);
}

TNode<HeapObject> CodeStubAssembler::LoadFastProperties(
    SloppyTNode<JSObject> object) {
  CSA_SLOW_ASSERT(this, Word32BinaryNot(IsDictionaryMap(LoadMap(object))));
  TNode<Object> properties = LoadJSReceiverPropertiesOrHash(object);
  return Select<HeapObject>(
      TaggedIsSmi(properties), [=] { return EmptyFixedArrayConstant(); },
      [=] { return CAST(properties); });
}

TNode<HeapObject> CodeStubAssembler::LoadSlowProperties(
    SloppyTNode<JSObject> object) {
  CSA_SLOW_ASSERT(this, IsDictionaryMap(LoadMap(object)));
  TNode<Object> properties = LoadJSReceiverPropertiesOrHash(object);
  return Select<HeapObject>(
      TaggedIsSmi(properties),
      [=] { return EmptyPropertyDictionaryConstant(); },
      [=] { return CAST(properties); });
}

TNode<Number> CodeStubAssembler::LoadJSArrayLength(SloppyTNode<JSArray> array) {
  CSA_ASSERT(this, IsJSArray(array));
  return CAST(LoadObjectField(array, JSArray::kLengthOffset));
}

TNode<Object> CodeStubAssembler::LoadJSArgumentsObjectWithLength(
    SloppyTNode<JSArgumentsObjectWithLength> array) {
  return LoadObjectField(array, JSArgumentsObjectWithLength::kLengthOffset);
}

TNode<Smi> CodeStubAssembler::LoadFastJSArrayLength(
    SloppyTNode<JSArray> array) {
  TNode<Number> length = LoadJSArrayLength(array);
  CSA_ASSERT(this, Word32Or(IsFastElementsKind(LoadElementsKind(array)),
                            IsElementsKindInRange(
                                LoadElementsKind(array),
                                FIRST_ANY_NONEXTENSIBLE_ELEMENTS_KIND,
                                LAST_ANY_NONEXTENSIBLE_ELEMENTS_KIND)));
  // JSArray length is always a positive Smi for fast arrays.
  CSA_SLOW_ASSERT(this, TaggedIsPositiveSmi(length));
  return UncheckedCast<Smi>(length);
}

TNode<Smi> CodeStubAssembler::LoadFixedArrayBaseLength(
    SloppyTNode<FixedArrayBase> array) {
  CSA_SLOW_ASSERT(this, IsNotWeakFixedArraySubclass(array));
  return CAST(LoadObjectField(array, FixedArrayBase::kLengthOffset));
}

TNode<IntPtrT> CodeStubAssembler::LoadAndUntagFixedArrayBaseLength(
    SloppyTNode<FixedArrayBase> array) {
  return LoadAndUntagObjectField(array, FixedArrayBase::kLengthOffset);
}

TNode<IntPtrT> CodeStubAssembler::LoadFeedbackVectorLength(
    TNode<FeedbackVector> vector) {
  return ChangeInt32ToIntPtr(
      LoadObjectField<Int32T>(vector, FeedbackVector::kLengthOffset));
}

TNode<Smi> CodeStubAssembler::LoadWeakFixedArrayLength(
    TNode<WeakFixedArray> array) {
  return LoadObjectField<Smi>(array, WeakFixedArray::kLengthOffset);
}

TNode<IntPtrT> CodeStubAssembler::LoadAndUntagWeakFixedArrayLength(
    SloppyTNode<WeakFixedArray> array) {
  return LoadAndUntagObjectField(array, WeakFixedArray::kLengthOffset);
}

TNode<Int32T> CodeStubAssembler::LoadNumberOfDescriptors(
    TNode<DescriptorArray> array) {
  return UncheckedCast<Int32T>(
      LoadObjectField(array, DescriptorArray::kNumberOfDescriptorsOffset,
                      MachineType::Int16()));
}

TNode<Int32T> CodeStubAssembler::LoadNumberOfOwnDescriptors(TNode<Map> map) {
  TNode<Uint32T> bit_field3 = LoadMapBitField3(map);
  return UncheckedCast<Int32T>(
      DecodeWord32<Map::NumberOfOwnDescriptorsBits>(bit_field3));
}

TNode<Int32T> CodeStubAssembler::LoadMapBitField(SloppyTNode<Map> map) {
  CSA_SLOW_ASSERT(this, IsMap(map));
  return UncheckedCast<Int32T>(
      LoadObjectField(map, Map::kBitFieldOffset, MachineType::Uint8()));
}

TNode<Int32T> CodeStubAssembler::LoadMapBitField2(SloppyTNode<Map> map) {
  CSA_SLOW_ASSERT(this, IsMap(map));
  return UncheckedCast<Int32T>(
      LoadObjectField(map, Map::kBitField2Offset, MachineType::Uint8()));
}

TNode<Uint32T> CodeStubAssembler::LoadMapBitField3(SloppyTNode<Map> map) {
  CSA_SLOW_ASSERT(this, IsMap(map));
  return UncheckedCast<Uint32T>(
      LoadObjectField(map, Map::kBitField3Offset, MachineType::Uint32()));
}

TNode<Uint16T> CodeStubAssembler::LoadMapInstanceType(SloppyTNode<Map> map) {
  return LoadObjectField<Uint16T>(map, Map::kInstanceTypeOffset);
}

TNode<Int32T> CodeStubAssembler::LoadMapElementsKind(SloppyTNode<Map> map) {
  CSA_SLOW_ASSERT(this, IsMap(map));
  TNode<Int32T> bit_field2 = LoadMapBitField2(map);
  return Signed(DecodeWord32<Map::ElementsKindBits>(bit_field2));
}

TNode<Int32T> CodeStubAssembler::LoadElementsKind(
    SloppyTNode<HeapObject> object) {
  return LoadMapElementsKind(LoadMap(object));
}

TNode<DescriptorArray> CodeStubAssembler::LoadMapDescriptors(
    SloppyTNode<Map> map) {
  CSA_SLOW_ASSERT(this, IsMap(map));
  return LoadObjectField<DescriptorArray>(map, Map::kInstanceDescriptorsOffset);
}

TNode<HeapObject> CodeStubAssembler::LoadMapPrototype(SloppyTNode<Map> map) {
  CSA_SLOW_ASSERT(this, IsMap(map));
  return LoadObjectField<HeapObject>(map, Map::kPrototypeOffset);
}

TNode<PrototypeInfo> CodeStubAssembler::LoadMapPrototypeInfo(
    SloppyTNode<Map> map, Label* if_no_proto_info) {
  Label if_strong_heap_object(this);
  CSA_ASSERT(this, IsMap(map));
  TNode<MaybeObject> maybe_prototype_info =
      LoadMaybeWeakObjectField(map, Map::kTransitionsOrPrototypeInfoOffset);
  TVARIABLE(Object, prototype_info);
  DispatchMaybeObject(maybe_prototype_info, if_no_proto_info, if_no_proto_info,
                      if_no_proto_info, &if_strong_heap_object,
                      &prototype_info);

  BIND(&if_strong_heap_object);
  GotoIfNot(TaggedEqual(LoadMap(CAST(prototype_info.value())),
                        PrototypeInfoMapConstant()),
            if_no_proto_info);
  return CAST(prototype_info.value());
}

TNode<IntPtrT> CodeStubAssembler::LoadMapInstanceSizeInWords(
    SloppyTNode<Map> map) {
  CSA_SLOW_ASSERT(this, IsMap(map));
  return ChangeInt32ToIntPtr(LoadObjectField(
      map, Map::kInstanceSizeInWordsOffset, MachineType::Uint8()));
}

TNode<IntPtrT> CodeStubAssembler::LoadMapInobjectPropertiesStartInWords(
    SloppyTNode<Map> map) {
  CSA_SLOW_ASSERT(this, IsMap(map));
  // See Map::GetInObjectPropertiesStartInWords() for details.
  CSA_ASSERT(this, IsJSObjectMap(map));
  return ChangeInt32ToIntPtr(LoadObjectField(
      map, Map::kInObjectPropertiesStartOrConstructorFunctionIndexOffset,
      MachineType::Uint8()));
}

TNode<IntPtrT> CodeStubAssembler::LoadMapConstructorFunctionIndex(
    SloppyTNode<Map> map) {
  CSA_SLOW_ASSERT(this, IsMap(map));
  // See Map::GetConstructorFunctionIndex() for details.
  CSA_ASSERT(this, IsPrimitiveInstanceType(LoadMapInstanceType(map)));
  return ChangeInt32ToIntPtr(LoadObjectField(
      map, Map::kInObjectPropertiesStartOrConstructorFunctionIndexOffset,
      MachineType::Uint8()));
}

TNode<Object> CodeStubAssembler::LoadMapConstructor(SloppyTNode<Map> map) {
  CSA_SLOW_ASSERT(this, IsMap(map));
  TVARIABLE(Object, result,
            LoadObjectField(map, Map::kConstructorOrBackPointerOffset));

  Label done(this), loop(this, &result);
  Goto(&loop);
  BIND(&loop);
  {
    GotoIf(TaggedIsSmi(result.value()), &done);
    TNode<BoolT> is_map_type =
        InstanceTypeEqual(LoadInstanceType(CAST(result.value())), MAP_TYPE);
    GotoIfNot(is_map_type, &done);
    result = LoadObjectField(CAST(result.value()),
                             Map::kConstructorOrBackPointerOffset);
    Goto(&loop);
  }
  BIND(&done);
  return result.value();
}

TNode<WordT> CodeStubAssembler::LoadMapEnumLength(SloppyTNode<Map> map) {
  CSA_SLOW_ASSERT(this, IsMap(map));
  TNode<Uint32T> bit_field3 = LoadMapBitField3(map);
  return DecodeWordFromWord32<Map::EnumLengthBits>(bit_field3);
}

TNode<Object> CodeStubAssembler::LoadMapBackPointer(SloppyTNode<Map> map) {
  TNode<HeapObject> object =
      CAST(LoadObjectField(map, Map::kConstructorOrBackPointerOffset));
  return Select<Object>(
      IsMap(object), [=] { return object; },
      [=] { return UndefinedConstant(); });
}

TNode<Uint32T> CodeStubAssembler::EnsureOnlyHasSimpleProperties(
    TNode<Map> map, TNode<Int32T> instance_type, Label* bailout) {
  // This check can have false positives, since it applies to any
  // JSPrimitiveWrapper type.
  GotoIf(IsCustomElementsReceiverInstanceType(instance_type), bailout);

  TNode<Uint32T> bit_field3 = LoadMapBitField3(map);
  GotoIf(IsSetWord32(bit_field3, Map::IsDictionaryMapBit::kMask), bailout);

  return bit_field3;
}

TNode<IntPtrT> CodeStubAssembler::LoadJSReceiverIdentityHash(
    SloppyTNode<Object> receiver, Label* if_no_hash) {
  TVARIABLE(IntPtrT, var_hash);
  Label done(this), if_smi(this), if_property_array(this),
      if_property_dictionary(this), if_fixed_array(this);

  TNode<Object> properties_or_hash =
      LoadObjectField(TNode<HeapObject>::UncheckedCast(receiver),
                      JSReceiver::kPropertiesOrHashOffset);
  GotoIf(TaggedIsSmi(properties_or_hash), &if_smi);

  TNode<HeapObject> properties =
      TNode<HeapObject>::UncheckedCast(properties_or_hash);
  TNode<Uint16T> properties_instance_type = LoadInstanceType(properties);

  GotoIf(InstanceTypeEqual(properties_instance_type, PROPERTY_ARRAY_TYPE),
         &if_property_array);
  Branch(InstanceTypeEqual(properties_instance_type, NAME_DICTIONARY_TYPE),
         &if_property_dictionary, &if_fixed_array);

  BIND(&if_fixed_array);
  {
    var_hash = IntPtrConstant(PropertyArray::kNoHashSentinel);
    Goto(&done);
  }

  BIND(&if_smi);
  {
    var_hash = SmiUntag(TNode<Smi>::UncheckedCast(properties_or_hash));
    Goto(&done);
  }

  BIND(&if_property_array);
  {
    TNode<IntPtrT> length_and_hash = LoadAndUntagObjectField(
        properties, PropertyArray::kLengthAndHashOffset);
    var_hash = TNode<IntPtrT>::UncheckedCast(
        DecodeWord<PropertyArray::HashField>(length_and_hash));
    Goto(&done);
  }

  BIND(&if_property_dictionary);
  {
    var_hash = SmiUntag(CAST(LoadFixedArrayElement(
        CAST(properties), NameDictionary::kObjectHashIndex)));
    Goto(&done);
  }

  BIND(&done);
  if (if_no_hash != nullptr) {
    GotoIf(IntPtrEqual(var_hash.value(),
                       IntPtrConstant(PropertyArray::kNoHashSentinel)),
           if_no_hash);
  }
  return var_hash.value();
}

TNode<Uint32T> CodeStubAssembler::LoadNameHashField(SloppyTNode<Name> name) {
  CSA_ASSERT(this, IsName(name));
  return LoadObjectField<Uint32T>(name, Name::kHashFieldOffset);
}

TNode<Uint32T> CodeStubAssembler::LoadNameHash(SloppyTNode<Name> name,
                                               Label* if_hash_not_computed) {
  TNode<Uint32T> hash_field = LoadNameHashField(name);
  if (if_hash_not_computed != nullptr) {
    GotoIf(IsSetWord32(hash_field, Name::kHashNotComputedMask),
           if_hash_not_computed);
  }
  return Unsigned(Word32Shr(hash_field, Int32Constant(Name::kHashShift)));
}

TNode<Smi> CodeStubAssembler::LoadStringLengthAsSmi(
    SloppyTNode<String> string) {
  return SmiFromIntPtr(LoadStringLengthAsWord(string));
}

TNode<IntPtrT> CodeStubAssembler::LoadStringLengthAsWord(
    SloppyTNode<String> string) {
  return Signed(ChangeUint32ToWord(LoadStringLengthAsWord32(string)));
}

TNode<Uint32T> CodeStubAssembler::LoadStringLengthAsWord32(
    SloppyTNode<String> string) {
  CSA_ASSERT(this, IsString(string));
  return LoadObjectField<Uint32T>(string, String::kLengthOffset);
}

Node* CodeStubAssembler::PointerToSeqStringData(Node* seq_string) {
  CSA_ASSERT(this, IsString(seq_string));
  CSA_ASSERT(this,
             IsSequentialStringInstanceType(LoadInstanceType(seq_string)));
  STATIC_ASSERT(SeqOneByteString::kHeaderSize == SeqTwoByteString::kHeaderSize);
  return IntPtrAdd(
      BitcastTaggedToWord(seq_string),
      IntPtrConstant(SeqOneByteString::kHeaderSize - kHeapObjectTag));
}

Node* CodeStubAssembler::LoadJSPrimitiveWrapperValue(Node* object) {
  CSA_ASSERT(this, IsJSPrimitiveWrapper(object));
  return LoadObjectField(object, JSPrimitiveWrapper::kValueOffset);
}

void CodeStubAssembler::DispatchMaybeObject(TNode<MaybeObject> maybe_object,
                                            Label* if_smi, Label* if_cleared,
                                            Label* if_weak, Label* if_strong,
                                            TVariable<Object>* extracted) {
  Label inner_if_smi(this), inner_if_strong(this);

  GotoIf(TaggedIsSmi(maybe_object), &inner_if_smi);

  GotoIf(IsCleared(maybe_object), if_cleared);

  GotoIf(Word32Equal(Word32And(TruncateIntPtrToInt32(
                                   BitcastMaybeObjectToWord(maybe_object)),
                               Int32Constant(kHeapObjectTagMask)),
                     Int32Constant(kHeapObjectTag)),
         &inner_if_strong);

  *extracted =
      BitcastWordToTagged(WordAnd(BitcastMaybeObjectToWord(maybe_object),
                                  IntPtrConstant(~kWeakHeapObjectMask)));
  Goto(if_weak);

  BIND(&inner_if_smi);
  *extracted = CAST(maybe_object);
  Goto(if_smi);

  BIND(&inner_if_strong);
  *extracted = CAST(maybe_object);
  Goto(if_strong);
}

TNode<BoolT> CodeStubAssembler::IsStrong(TNode<MaybeObject> value) {
  return Word32Equal(
      Word32And(TruncateIntPtrToInt32(BitcastMaybeObjectToWord(value)),
                Int32Constant(kHeapObjectTagMask)),
      Int32Constant(kHeapObjectTag));
}

TNode<HeapObject> CodeStubAssembler::GetHeapObjectIfStrong(
    TNode<MaybeObject> value, Label* if_not_strong) {
  GotoIfNot(IsStrong(value), if_not_strong);
  return CAST(value);
}

TNode<BoolT> CodeStubAssembler::IsWeakOrCleared(TNode<MaybeObject> value) {
  return Word32Equal(
      Word32And(TruncateIntPtrToInt32(BitcastMaybeObjectToWord(value)),
                Int32Constant(kHeapObjectTagMask)),
      Int32Constant(kWeakHeapObjectTag));
}

TNode<BoolT> CodeStubAssembler::IsCleared(TNode<MaybeObject> value) {
  return Word32Equal(TruncateIntPtrToInt32(BitcastMaybeObjectToWord(value)),
                     Int32Constant(kClearedWeakHeapObjectLower32));
}

TNode<BoolT> CodeStubAssembler::IsNotCleared(TNode<MaybeObject> value) {
  return Word32NotEqual(TruncateIntPtrToInt32(BitcastMaybeObjectToWord(value)),
                        Int32Constant(kClearedWeakHeapObjectLower32));
}

TNode<HeapObject> CodeStubAssembler::GetHeapObjectAssumeWeak(
    TNode<MaybeObject> value) {
  CSA_ASSERT(this, IsWeakOrCleared(value));
  CSA_ASSERT(this, IsNotCleared(value));
  return UncheckedCast<HeapObject>(BitcastWordToTagged(WordAnd(
      BitcastMaybeObjectToWord(value), IntPtrConstant(~kWeakHeapObjectMask))));
}

TNode<HeapObject> CodeStubAssembler::GetHeapObjectAssumeWeak(
    TNode<MaybeObject> value, Label* if_cleared) {
  GotoIf(IsCleared(value), if_cleared);
  return GetHeapObjectAssumeWeak(value);
}

TNode<BoolT> CodeStubAssembler::IsWeakReferenceTo(TNode<MaybeObject> object,
                                                  TNode<Object> value) {
#if defined(V8_HOST_ARCH_32_BIT) || defined(V8_COMPRESS_POINTERS)
  STATIC_ASSERT(kTaggedSize == kInt32Size);
  return Word32Equal(
      Word32And(TruncateWordToInt32(BitcastMaybeObjectToWord(object)),
                Uint32Constant(
                    static_cast<uint32_t>(~kWeakHeapObjectMask & kMaxUInt32))),
      TruncateWordToInt32(BitcastTaggedToWord(value)));
#else
  return WordEqual(WordAnd(BitcastMaybeObjectToWord(object),
                           IntPtrConstant(~kWeakHeapObjectMask)),
                   BitcastTaggedToWord(value));

#endif
}

TNode<BoolT> CodeStubAssembler::IsStrongReferenceTo(TNode<MaybeObject> object,
                                                    TNode<Object> value) {
  return TaggedEqual(BitcastWordToTagged(BitcastMaybeObjectToWord(object)),
                     value);
}

TNode<BoolT> CodeStubAssembler::IsNotWeakReferenceTo(TNode<MaybeObject> object,
                                                     TNode<Object> value) {
#if defined(V8_HOST_ARCH_32_BIT) || defined(V8_COMPRESS_POINTERS)
  return Word32NotEqual(
      Word32And(TruncateWordToInt32(BitcastMaybeObjectToWord(object)),
                Uint32Constant(
                    static_cast<uint32_t>(~kWeakHeapObjectMask & kMaxUInt32))),
      TruncateWordToInt32(BitcastTaggedToWord(value)));
#else
  return WordNotEqual(WordAnd(BitcastMaybeObjectToWord(object),
                              IntPtrConstant(~kWeakHeapObjectMask)),
                      BitcastTaggedToWord(value));

#endif
}

TNode<MaybeObject> CodeStubAssembler::MakeWeak(TNode<HeapObject> value) {
  return ReinterpretCast<MaybeObject>(BitcastWordToTagged(
      WordOr(BitcastTaggedToWord(value), IntPtrConstant(kWeakHeapObjectTag))));
}

template <>
TNode<IntPtrT> CodeStubAssembler::LoadArrayLength(TNode<FixedArray> array) {
  return LoadAndUntagFixedArrayBaseLength(array);
}

template <>
TNode<IntPtrT> CodeStubAssembler::LoadArrayLength(TNode<WeakFixedArray> array) {
  return LoadAndUntagWeakFixedArrayLength(array);
}

template <>
TNode<IntPtrT> CodeStubAssembler::LoadArrayLength(TNode<PropertyArray> array) {
  return LoadPropertyArrayLength(array);
}

template <>
TNode<IntPtrT> CodeStubAssembler::LoadArrayLength(
    TNode<DescriptorArray> array) {
  return IntPtrMul(ChangeInt32ToIntPtr(LoadNumberOfDescriptors(array)),
                   IntPtrConstant(DescriptorArray::kEntrySize));
}

template <>
TNode<IntPtrT> CodeStubAssembler::LoadArrayLength(
    TNode<TransitionArray> array) {
  return LoadAndUntagWeakFixedArrayLength(array);
}

template <typename Array, typename T>
TNode<T> CodeStubAssembler::LoadArrayElement(TNode<Array> array,
                                             int array_header_size,
                                             Node* index_node,
                                             int additional_offset,
                                             ParameterMode parameter_mode,
                                             LoadSensitivity needs_poisoning) {
  CSA_ASSERT(this, IntPtrGreaterThanOrEqual(
                       ParameterToIntPtr(index_node, parameter_mode),
                       IntPtrConstant(0)));
  DCHECK(IsAligned(additional_offset, kTaggedSize));
  int32_t header_size = array_header_size + additional_offset - kHeapObjectTag;
  TNode<IntPtrT> offset = ElementOffsetFromIndex(index_node, HOLEY_ELEMENTS,
                                                 parameter_mode, header_size);
  CSA_ASSERT(this, IsOffsetInBounds(offset, LoadArrayLength(array),
                                    array_header_size));
  constexpr MachineType machine_type = MachineTypeOf<T>::value;
  // TODO(gsps): Remove the Load case once LoadFromObject supports poisoning
  if (needs_poisoning == LoadSensitivity::kSafe) {
    return UncheckedCast<T>(LoadFromObject(machine_type, array, offset));
  } else {
    return UncheckedCast<T>(Load(machine_type, array, offset, needs_poisoning));
  }
}

template TNode<MaybeObject>
CodeStubAssembler::LoadArrayElement<TransitionArray>(TNode<TransitionArray>,
                                                     int, Node*, int,
                                                     ParameterMode,
                                                     LoadSensitivity);

template TNode<MaybeObject>
CodeStubAssembler::LoadArrayElement<DescriptorArray>(TNode<DescriptorArray>,
                                                     int, Node*, int,
                                                     ParameterMode,
                                                     LoadSensitivity);

void CodeStubAssembler::FixedArrayBoundsCheck(TNode<FixedArrayBase> array,
                                              Node* index,
                                              int additional_offset,
                                              ParameterMode parameter_mode) {
  if (!FLAG_fixed_array_bounds_checks) return;
  DCHECK(IsAligned(additional_offset, kTaggedSize));
  if (parameter_mode == ParameterMode::SMI_PARAMETERS) {
    TNode<Smi> effective_index;
    Smi constant_index;
    bool index_is_constant = ToSmiConstant(index, &constant_index);
    if (index_is_constant) {
      effective_index = SmiConstant(Smi::ToInt(constant_index) +
                                    additional_offset / kTaggedSize);
    } else if (additional_offset != 0) {
      effective_index =
          SmiAdd(CAST(index), SmiConstant(additional_offset / kTaggedSize));
    } else {
      effective_index = CAST(index);
    }
    CSA_CHECK(this, SmiBelow(effective_index, LoadFixedArrayBaseLength(array)));
  } else {
    // IntPtrAdd does constant-folding automatically.
    TNode<IntPtrT> effective_index =
        IntPtrAdd(UncheckedCast<IntPtrT>(index),
                  IntPtrConstant(additional_offset / kTaggedSize));
    CSA_CHECK(this, UintPtrLessThan(effective_index,
                                    LoadAndUntagFixedArrayBaseLength(array)));
  }
}

TNode<Object> CodeStubAssembler::LoadFixedArrayElement(
    TNode<FixedArray> object, Node* index_node, int additional_offset,
    ParameterMode parameter_mode, LoadSensitivity needs_poisoning,
    CheckBounds check_bounds) {
  CSA_ASSERT(this, IsFixedArraySubclass(object));
  CSA_ASSERT(this, IsNotWeakFixedArraySubclass(object));
  if (NeedsBoundsCheck(check_bounds)) {
    FixedArrayBoundsCheck(object, index_node, additional_offset,
                          parameter_mode);
  }
  TNode<MaybeObject> element =
      LoadArrayElement(object, FixedArray::kHeaderSize, index_node,
                       additional_offset, parameter_mode, needs_poisoning);
  return CAST(element);
}

TNode<Object> CodeStubAssembler::LoadPropertyArrayElement(
    TNode<PropertyArray> object, SloppyTNode<IntPtrT> index) {
  int additional_offset = 0;
  ParameterMode parameter_mode = INTPTR_PARAMETERS;
  LoadSensitivity needs_poisoning = LoadSensitivity::kSafe;
  return CAST(LoadArrayElement(object, PropertyArray::kHeaderSize, index,
                               additional_offset, parameter_mode,
                               needs_poisoning));
}

TNode<IntPtrT> CodeStubAssembler::LoadPropertyArrayLength(
    TNode<PropertyArray> object) {
  TNode<IntPtrT> value =
      LoadAndUntagObjectField(object, PropertyArray::kLengthAndHashOffset);
  return Signed(DecodeWord<PropertyArray::LengthField>(value));
}

TNode<RawPtrT> CodeStubAssembler::LoadJSTypedArrayBackingStore(
    TNode<JSTypedArray> typed_array) {
  // Backing store = external_pointer + base_pointer.
  Node* external_pointer =
      LoadObjectField(typed_array, JSTypedArray::kExternalPointerOffset,
                      MachineType::Pointer());
  TNode<Object> base_pointer =
      LoadObjectField(typed_array, JSTypedArray::kBasePointerOffset);
  return UncheckedCast<RawPtrT>(
      IntPtrAdd(external_pointer, BitcastTaggedToWord(base_pointer)));
}

TNode<BigInt> CodeStubAssembler::LoadFixedBigInt64ArrayElementAsTagged(
    SloppyTNode<RawPtrT> data_pointer, SloppyTNode<IntPtrT> offset) {
  if (Is64()) {
    TNode<IntPtrT> value = UncheckedCast<IntPtrT>(
        Load(MachineType::IntPtr(), data_pointer, offset));
    return BigIntFromInt64(value);
  } else {
    DCHECK(!Is64());
#if defined(V8_TARGET_BIG_ENDIAN)
    TNode<IntPtrT> high = UncheckedCast<IntPtrT>(
        Load(MachineType::UintPtr(), data_pointer, offset));
    TNode<IntPtrT> low = UncheckedCast<IntPtrT>(
        Load(MachineType::UintPtr(), data_pointer,
             Int32Add(TruncateIntPtrToInt32(offset),
                      Int32Constant(kSystemPointerSize))));
#else
    TNode<IntPtrT> low = UncheckedCast<IntPtrT>(
        Load(MachineType::UintPtr(), data_pointer, offset));
    TNode<IntPtrT> high = UncheckedCast<IntPtrT>(
        Load(MachineType::UintPtr(), data_pointer,
             Int32Add(TruncateIntPtrToInt32(offset),
                      Int32Constant(kSystemPointerSize))));
#endif
    return BigIntFromInt32Pair(low, high);
  }
}

TNode<BigInt> CodeStubAssembler::BigIntFromInt32Pair(TNode<IntPtrT> low,
                                                     TNode<IntPtrT> high) {
  DCHECK(!Is64());
  TVARIABLE(BigInt, var_result);
  TVARIABLE(Word32T, var_sign, Int32Constant(BigInt::SignBits::encode(false)));
  TVARIABLE(IntPtrT, var_high, high);
  TVARIABLE(IntPtrT, var_low, low);
  Label high_zero(this), negative(this), allocate_one_digit(this),
      allocate_two_digits(this), if_zero(this), done(this);

  GotoIf(IntPtrEqual(var_high.value(), IntPtrConstant(0)), &high_zero);
  Branch(IntPtrLessThan(var_high.value(), IntPtrConstant(0)), &negative,
         &allocate_two_digits);

  BIND(&high_zero);
  Branch(IntPtrEqual(var_low.value(), IntPtrConstant(0)), &if_zero,
         &allocate_one_digit);

  BIND(&negative);
  {
    var_sign = Int32Constant(BigInt::SignBits::encode(true));
    // We must negate the value by computing "0 - (high|low)", performing
    // both parts of the subtraction separately and manually taking care
    // of the carry bit (which is 1 iff low != 0).
    var_high = IntPtrSub(IntPtrConstant(0), var_high.value());
    Label carry(this), no_carry(this);
    Branch(IntPtrEqual(var_low.value(), IntPtrConstant(0)), &no_carry, &carry);
    BIND(&carry);
    var_high = IntPtrSub(var_high.value(), IntPtrConstant(1));
    Goto(&no_carry);
    BIND(&no_carry);
    var_low = IntPtrSub(IntPtrConstant(0), var_low.value());
    // var_high was non-zero going into this block, but subtracting the
    // carry bit from it could bring us back onto the "one digit" path.
    Branch(IntPtrEqual(var_high.value(), IntPtrConstant(0)),
           &allocate_one_digit, &allocate_two_digits);
  }

  BIND(&allocate_one_digit);
  {
    var_result = AllocateRawBigInt(IntPtrConstant(1));
    StoreBigIntBitfield(var_result.value(),
                        Word32Or(var_sign.value(),
                                 Int32Constant(BigInt::LengthBits::encode(1))));
    StoreBigIntDigit(var_result.value(), 0, Unsigned(var_low.value()));
    Goto(&done);
  }

  BIND(&allocate_two_digits);
  {
    var_result = AllocateRawBigInt(IntPtrConstant(2));
    StoreBigIntBitfield(var_result.value(),
                        Word32Or(var_sign.value(),
                                 Int32Constant(BigInt::LengthBits::encode(2))));
    StoreBigIntDigit(var_result.value(), 0, Unsigned(var_low.value()));
    StoreBigIntDigit(var_result.value(), 1, Unsigned(var_high.value()));
    Goto(&done);
  }

  BIND(&if_zero);
  var_result = AllocateBigInt(IntPtrConstant(0));
  Goto(&done);

  BIND(&done);
  return var_result.value();
}

TNode<BigInt> CodeStubAssembler::BigIntFromInt64(TNode<IntPtrT> value) {
  DCHECK(Is64());
  TVARIABLE(BigInt, var_result);
  Label done(this), if_positive(this), if_negative(this), if_zero(this);
  GotoIf(IntPtrEqual(value, IntPtrConstant(0)), &if_zero);
  var_result = AllocateRawBigInt(IntPtrConstant(1));
  Branch(IntPtrGreaterThan(value, IntPtrConstant(0)), &if_positive,
         &if_negative);

  BIND(&if_positive);
  {
    StoreBigIntBitfield(var_result.value(),
                        Int32Constant(BigInt::SignBits::encode(false) |
                                      BigInt::LengthBits::encode(1)));
    StoreBigIntDigit(var_result.value(), 0, Unsigned(value));
    Goto(&done);
  }

  BIND(&if_negative);
  {
    StoreBigIntBitfield(var_result.value(),
                        Int32Constant(BigInt::SignBits::encode(true) |
                                      BigInt::LengthBits::encode(1)));
    StoreBigIntDigit(var_result.value(), 0,
                     Unsigned(IntPtrSub(IntPtrConstant(0), value)));
    Goto(&done);
  }

  BIND(&if_zero);
  {
    var_result = AllocateBigInt(IntPtrConstant(0));
    Goto(&done);
  }

  BIND(&done);
  return var_result.value();
}

compiler::TNode<BigInt>
CodeStubAssembler::LoadFixedBigUint64ArrayElementAsTagged(
    SloppyTNode<RawPtrT> data_pointer, SloppyTNode<IntPtrT> offset) {
  Label if_zero(this), done(this);
  if (Is64()) {
    TNode<UintPtrT> value = UncheckedCast<UintPtrT>(
        Load(MachineType::UintPtr(), data_pointer, offset));
    return BigIntFromUint64(value);
  } else {
    DCHECK(!Is64());
#if defined(V8_TARGET_BIG_ENDIAN)
    TNode<UintPtrT> high = UncheckedCast<UintPtrT>(
        Load(MachineType::UintPtr(), data_pointer, offset));
    TNode<UintPtrT> low = UncheckedCast<UintPtrT>(
        Load(MachineType::UintPtr(), data_pointer,
             Int32Add(TruncateIntPtrToInt32(offset),
                      Int32Constant(kSystemPointerSize))));
#else
    TNode<UintPtrT> low = UncheckedCast<UintPtrT>(
        Load(MachineType::UintPtr(), data_pointer, offset));
    TNode<UintPtrT> high = UncheckedCast<UintPtrT>(
        Load(MachineType::UintPtr(), data_pointer,
             Int32Add(TruncateIntPtrToInt32(offset),
                      Int32Constant(kSystemPointerSize))));
#endif
    return BigIntFromUint32Pair(low, high);
  }
}

TNode<BigInt> CodeStubAssembler::BigIntFromUint32Pair(TNode<UintPtrT> low,
                                                      TNode<UintPtrT> high) {
  DCHECK(!Is64());
  TVARIABLE(BigInt, var_result);
  Label high_zero(this), if_zero(this), done(this);

  GotoIf(IntPtrEqual(high, IntPtrConstant(0)), &high_zero);
  var_result = AllocateBigInt(IntPtrConstant(2));
  StoreBigIntDigit(var_result.value(), 0, low);
  StoreBigIntDigit(var_result.value(), 1, high);
  Goto(&done);

  BIND(&high_zero);
  GotoIf(IntPtrEqual(low, IntPtrConstant(0)), &if_zero);
  var_result = AllocateBigInt(IntPtrConstant(1));
  StoreBigIntDigit(var_result.value(), 0, low);
  Goto(&done);

  BIND(&if_zero);
  var_result = AllocateBigInt(IntPtrConstant(0));
  Goto(&done);

  BIND(&done);
  return var_result.value();
}

TNode<BigInt> CodeStubAssembler::BigIntFromUint64(TNode<UintPtrT> value) {
  DCHECK(Is64());
  TVARIABLE(BigInt, var_result);
  Label done(this), if_zero(this);
  GotoIf(IntPtrEqual(value, IntPtrConstant(0)), &if_zero);
  var_result = AllocateBigInt(IntPtrConstant(1));
  StoreBigIntDigit(var_result.value(), 0, value);
  Goto(&done);

  BIND(&if_zero);
  var_result = AllocateBigInt(IntPtrConstant(0));
  Goto(&done);
  BIND(&done);
  return var_result.value();
}

TNode<Numeric> CodeStubAssembler::LoadFixedTypedArrayElementAsTagged(
    TNode<RawPtrT> data_pointer, Node* index_node, ElementsKind elements_kind,
    ParameterMode parameter_mode) {
  TNode<IntPtrT> offset =
      ElementOffsetFromIndex(index_node, elements_kind, parameter_mode, 0);
  switch (elements_kind) {
    case UINT8_ELEMENTS: /* fall through */
    case UINT8_CLAMPED_ELEMENTS:
      return SmiFromInt32(Load(MachineType::Uint8(), data_pointer, offset));
    case INT8_ELEMENTS:
      return SmiFromInt32(Load(MachineType::Int8(), data_pointer, offset));
    case UINT16_ELEMENTS:
      return SmiFromInt32(Load(MachineType::Uint16(), data_pointer, offset));
    case INT16_ELEMENTS:
      return SmiFromInt32(Load(MachineType::Int16(), data_pointer, offset));
    case UINT32_ELEMENTS:
      return ChangeUint32ToTagged(
          Load(MachineType::Uint32(), data_pointer, offset));
    case INT32_ELEMENTS:
      return ChangeInt32ToTagged(
          Load(MachineType::Int32(), data_pointer, offset));
    case FLOAT32_ELEMENTS:
      return AllocateHeapNumberWithValue(ChangeFloat32ToFloat64(
          Load(MachineType::Float32(), data_pointer, offset)));
    case FLOAT64_ELEMENTS:
      return AllocateHeapNumberWithValue(
          Load(MachineType::Float64(), data_pointer, offset));
    case BIGINT64_ELEMENTS:
      return LoadFixedBigInt64ArrayElementAsTagged(data_pointer, offset);
    case BIGUINT64_ELEMENTS:
      return LoadFixedBigUint64ArrayElementAsTagged(data_pointer, offset);
    default:
      UNREACHABLE();
  }
}

TNode<Numeric> CodeStubAssembler::LoadFixedTypedArrayElementAsTagged(
    TNode<RawPtrT> data_pointer, TNode<Smi> index,
    TNode<Int32T> elements_kind) {
  TVARIABLE(Numeric, var_result);
  Label done(this), if_unknown_type(this, Label::kDeferred);
  int32_t elements_kinds[] = {
#define TYPED_ARRAY_CASE(Type, type, TYPE, ctype) TYPE##_ELEMENTS,
      TYPED_ARRAYS(TYPED_ARRAY_CASE)
#undef TYPED_ARRAY_CASE
  };

#define TYPED_ARRAY_CASE(Type, type, TYPE, ctype) Label if_##type##array(this);
  TYPED_ARRAYS(TYPED_ARRAY_CASE)
#undef TYPED_ARRAY_CASE

  Label* elements_kind_labels[] = {
#define TYPED_ARRAY_CASE(Type, type, TYPE, ctype) &if_##type##array,
      TYPED_ARRAYS(TYPED_ARRAY_CASE)
#undef TYPED_ARRAY_CASE
  };
  STATIC_ASSERT(arraysize(elements_kinds) == arraysize(elements_kind_labels));

  Switch(elements_kind, &if_unknown_type, elements_kinds, elements_kind_labels,
         arraysize(elements_kinds));

  BIND(&if_unknown_type);
  Unreachable();

#define TYPED_ARRAY_CASE(Type, type, TYPE, ctype)              \
  BIND(&if_##type##array);                                     \
  {                                                            \
    var_result = LoadFixedTypedArrayElementAsTagged(           \
        data_pointer, index, TYPE##_ELEMENTS, SMI_PARAMETERS); \
    Goto(&done);                                               \
  }
  TYPED_ARRAYS(TYPED_ARRAY_CASE)
#undef TYPED_ARRAY_CASE

  BIND(&done);
  return var_result.value();
}

void CodeStubAssembler::StoreJSTypedArrayElementFromTagged(
    TNode<Context> context, TNode<JSTypedArray> typed_array,
    TNode<Smi> index_node, TNode<Object> value, ElementsKind elements_kind) {
  TNode<RawPtrT> data_pointer = LoadJSTypedArrayBackingStore(typed_array);
  switch (elements_kind) {
    case UINT8_ELEMENTS:
    case UINT8_CLAMPED_ELEMENTS:
    case INT8_ELEMENTS:
    case UINT16_ELEMENTS:
    case INT16_ELEMENTS:
      StoreElement(data_pointer, elements_kind, index_node,
                   SmiToInt32(CAST(value)), SMI_PARAMETERS);
      break;
    case UINT32_ELEMENTS:
    case INT32_ELEMENTS:
      StoreElement(data_pointer, elements_kind, index_node,
                   TruncateTaggedToWord32(context, value), SMI_PARAMETERS);
      break;
    case FLOAT32_ELEMENTS:
      StoreElement(data_pointer, elements_kind, index_node,
                   TruncateFloat64ToFloat32(LoadHeapNumberValue(CAST(value))),
                   SMI_PARAMETERS);
      break;
    case FLOAT64_ELEMENTS:
      StoreElement(data_pointer, elements_kind, index_node,
                   LoadHeapNumberValue(CAST(value)), SMI_PARAMETERS);
      break;
    case BIGUINT64_ELEMENTS:
    case BIGINT64_ELEMENTS:
      StoreElement(data_pointer, elements_kind, index_node,
                   UncheckedCast<BigInt>(value), SMI_PARAMETERS);
      break;
    default:
      UNREACHABLE();
  }
}

TNode<MaybeObject> CodeStubAssembler::LoadFeedbackVectorSlot(
    Node* object, Node* slot_index_node, int additional_offset,
    ParameterMode parameter_mode) {
  CSA_SLOW_ASSERT(this, IsFeedbackVector(object));
  CSA_SLOW_ASSERT(this, MatchesParameterMode(slot_index_node, parameter_mode));
  int32_t header_size =
      FeedbackVector::kFeedbackSlotsOffset + additional_offset - kHeapObjectTag;
  TNode<IntPtrT> offset = ElementOffsetFromIndex(
      slot_index_node, HOLEY_ELEMENTS, parameter_mode, header_size);
  CSA_SLOW_ASSERT(
      this, IsOffsetInBounds(offset, LoadFeedbackVectorLength(CAST(object)),
                             FeedbackVector::kHeaderSize));
  return UncheckedCast<MaybeObject>(
      Load(MachineType::AnyTagged(), object, offset));
}

template <typename Array>
TNode<Int32T> CodeStubAssembler::LoadAndUntagToWord32ArrayElement(
    TNode<Array> object, int array_header_size, Node* index_node,
    int additional_offset, ParameterMode parameter_mode) {
  CSA_SLOW_ASSERT(this, MatchesParameterMode(index_node, parameter_mode));
  DCHECK(IsAligned(additional_offset, kTaggedSize));
  int endian_correction = 0;
#if V8_TARGET_LITTLE_ENDIAN
  if (SmiValuesAre32Bits()) endian_correction = 4;
#endif
  int32_t header_size = array_header_size + additional_offset - kHeapObjectTag +
                        endian_correction;
  TNode<IntPtrT> offset = ElementOffsetFromIndex(index_node, HOLEY_ELEMENTS,
                                                 parameter_mode, header_size);
  CSA_ASSERT(this, IsOffsetInBounds(offset, LoadArrayLength(object),
                                    array_header_size + endian_correction));
  if (SmiValuesAre32Bits()) {
    return UncheckedCast<Int32T>(Load(MachineType::Int32(), object, offset));
  } else {
    return SmiToInt32(Load(MachineType::TaggedSigned(), object, offset));
  }
}

TNode<Int32T> CodeStubAssembler::LoadAndUntagToWord32FixedArrayElement(
    TNode<FixedArray> object, Node* index_node, int additional_offset,
    ParameterMode parameter_mode) {
  CSA_SLOW_ASSERT(this, IsFixedArraySubclass(object));
  return LoadAndUntagToWord32ArrayElement(object, FixedArray::kHeaderSize,
                                          index_node, additional_offset,
                                          parameter_mode);
}

TNode<MaybeObject> CodeStubAssembler::LoadWeakFixedArrayElement(
    TNode<WeakFixedArray> object, Node* index, int additional_offset,
    ParameterMode parameter_mode, LoadSensitivity needs_poisoning) {
  return LoadArrayElement(object, WeakFixedArray::kHeaderSize, index,
                          additional_offset, parameter_mode, needs_poisoning);
}

TNode<Float64T> CodeStubAssembler::LoadFixedDoubleArrayElement(
    SloppyTNode<FixedDoubleArray> object, Node* index_node,
    MachineType machine_type, int additional_offset,
    ParameterMode parameter_mode, Label* if_hole) {
  CSA_ASSERT(this, IsFixedDoubleArray(object));
  DCHECK(IsAligned(additional_offset, kTaggedSize));
  CSA_SLOW_ASSERT(this, MatchesParameterMode(index_node, parameter_mode));
  int32_t header_size =
      FixedDoubleArray::kHeaderSize + additional_offset - kHeapObjectTag;
  TNode<IntPtrT> offset = ElementOffsetFromIndex(
      index_node, HOLEY_DOUBLE_ELEMENTS, parameter_mode, header_size);
  CSA_ASSERT(this, IsOffsetInBounds(
                       offset, LoadAndUntagFixedArrayBaseLength(object),
                       FixedDoubleArray::kHeaderSize, HOLEY_DOUBLE_ELEMENTS));
  return LoadDoubleWithHoleCheck(object, offset, if_hole, machine_type);
}

TNode<Object> CodeStubAssembler::LoadFixedArrayBaseElementAsTagged(
    TNode<FixedArrayBase> elements, TNode<IntPtrT> index,
    TNode<Int32T> elements_kind, Label* if_accessor, Label* if_hole) {
  TVARIABLE(Object, var_result);
  Label done(this), if_packed(this), if_holey(this), if_packed_double(this),
      if_holey_double(this), if_dictionary(this, Label::kDeferred);

  int32_t kinds[] = {
      // Handled by if_packed.
      PACKED_SMI_ELEMENTS, PACKED_ELEMENTS, PACKED_NONEXTENSIBLE_ELEMENTS,
      PACKED_SEALED_ELEMENTS, PACKED_FROZEN_ELEMENTS,
      // Handled by if_holey.
      HOLEY_SMI_ELEMENTS, HOLEY_ELEMENTS, HOLEY_NONEXTENSIBLE_ELEMENTS,
      HOLEY_SEALED_ELEMENTS, HOLEY_FROZEN_ELEMENTS,
      // Handled by if_packed_double.
      PACKED_DOUBLE_ELEMENTS,
      // Handled by if_holey_double.
      HOLEY_DOUBLE_ELEMENTS};
  Label* labels[] = {// PACKED_{SMI,}_ELEMENTS
                     &if_packed, &if_packed, &if_packed, &if_packed, &if_packed,
                     // HOLEY_{SMI,}_ELEMENTS
                     &if_holey, &if_holey, &if_holey, &if_holey, &if_holey,
                     // PACKED_DOUBLE_ELEMENTS
                     &if_packed_double,
                     // HOLEY_DOUBLE_ELEMENTS
                     &if_holey_double};
  Switch(elements_kind, &if_dictionary, kinds, labels, arraysize(kinds));

  BIND(&if_packed);
  {
    var_result = LoadFixedArrayElement(CAST(elements), index, 0);
    Goto(&done);
  }

  BIND(&if_holey);
  {
    var_result = LoadFixedArrayElement(CAST(elements), index);
    Branch(TaggedEqual(var_result.value(), TheHoleConstant()), if_hole, &done);
  }

  BIND(&if_packed_double);
  {
    var_result = AllocateHeapNumberWithValue(LoadFixedDoubleArrayElement(
        CAST(elements), index, MachineType::Float64()));
    Goto(&done);
  }

  BIND(&if_holey_double);
  {
    var_result = AllocateHeapNumberWithValue(LoadFixedDoubleArrayElement(
        CAST(elements), index, MachineType::Float64(), 0, INTPTR_PARAMETERS,
        if_hole));
    Goto(&done);
  }

  BIND(&if_dictionary);
  {
    CSA_ASSERT(this, IsDictionaryElementsKind(elements_kind));
    var_result = BasicLoadNumberDictionaryElement(CAST(elements), index,
                                                  if_accessor, if_hole);
    Goto(&done);
  }

  BIND(&done);
  return var_result.value();
}

TNode<Float64T> CodeStubAssembler::LoadDoubleWithHoleCheck(
    SloppyTNode<Object> base, SloppyTNode<IntPtrT> offset, Label* if_hole,
    MachineType machine_type) {
  if (if_hole) {
    // TODO(ishell): Compare only the upper part for the hole once the
    // compiler is able to fold addition of already complex |offset| with
    // |kIeeeDoubleExponentWordOffset| into one addressing mode.
    if (Is64()) {
      TNode<Uint64T> element = Load<Uint64T>(base, offset);
      GotoIf(Word64Equal(element, Int64Constant(kHoleNanInt64)), if_hole);
    } else {
      TNode<Uint32T> element_upper = Load<Uint32T>(
          base,
          IntPtrAdd(offset, IntPtrConstant(kIeeeDoubleExponentWordOffset)));
      GotoIf(Word32Equal(element_upper, Int32Constant(kHoleNanUpper32)),
             if_hole);
    }
  }
  if (machine_type.IsNone()) {
    // This means the actual value is not needed.
    return TNode<Float64T>();
  }
  return UncheckedCast<Float64T>(Load(machine_type, base, offset));
}

TNode<Object> CodeStubAssembler::LoadContextElement(
    SloppyTNode<Context> context, int slot_index) {
  int offset = Context::SlotOffset(slot_index);
  return UncheckedCast<Object>(
      Load(MachineType::AnyTagged(), context, IntPtrConstant(offset)));
}

TNode<Object> CodeStubAssembler::LoadContextElement(
    SloppyTNode<Context> context, SloppyTNode<IntPtrT> slot_index) {
  TNode<IntPtrT> offset = ElementOffsetFromIndex(
      slot_index, PACKED_ELEMENTS, INTPTR_PARAMETERS, Context::SlotOffset(0));
  return UncheckedCast<Object>(Load(MachineType::AnyTagged(), context, offset));
}

TNode<Object> CodeStubAssembler::LoadContextElement(TNode<Context> context,
                                                    TNode<Smi> slot_index) {
  TNode<IntPtrT> offset = ElementOffsetFromIndex(
      slot_index, PACKED_ELEMENTS, SMI_PARAMETERS, Context::SlotOffset(0));
  return UncheckedCast<Object>(Load(MachineType::AnyTagged(), context, offset));
}

void CodeStubAssembler::StoreContextElement(SloppyTNode<Context> context,
                                            int slot_index,
                                            SloppyTNode<Object> value) {
  int offset = Context::SlotOffset(slot_index);
  Store(context, IntPtrConstant(offset), value);
}

void CodeStubAssembler::StoreContextElement(SloppyTNode<Context> context,
                                            SloppyTNode<IntPtrT> slot_index,
                                            SloppyTNode<Object> value) {
  TNode<IntPtrT> offset = IntPtrAdd(TimesTaggedSize(slot_index),
                                    IntPtrConstant(Context::SlotOffset(0)));
  Store(context, offset, value);
}

void CodeStubAssembler::StoreContextElementNoWriteBarrier(
    SloppyTNode<Context> context, int slot_index, SloppyTNode<Object> value) {
  int offset = Context::SlotOffset(slot_index);
  StoreNoWriteBarrier(MachineRepresentation::kTagged, context,
                      IntPtrConstant(offset), value);
}

TNode<NativeContext> CodeStubAssembler::LoadNativeContext(
    SloppyTNode<Context> context) {
  return UncheckedCast<NativeContext>(
      LoadContextElement(context, Context::NATIVE_CONTEXT_INDEX));
}

TNode<Context> CodeStubAssembler::LoadModuleContext(
    SloppyTNode<Context> context) {
  TNode<Map> module_map = ModuleContextMapConstant();
  Variable cur_context(this, MachineRepresentation::kTaggedPointer);
  cur_context.Bind(context);

  Label context_found(this);

  Variable* context_search_loop_variables[1] = {&cur_context};
  Label context_search(this, 1, context_search_loop_variables);

  // Loop until cur_context->map() is module_map.
  Goto(&context_search);
  BIND(&context_search);
  {
    CSA_ASSERT(this, Word32BinaryNot(IsNativeContext(cur_context.value())));
    GotoIf(TaggedEqual(LoadMap(cur_context.value()), module_map),
           &context_found);

    cur_context.Bind(
        LoadContextElement(cur_context.value(), Context::PREVIOUS_INDEX));
    Goto(&context_search);
  }

  BIND(&context_found);
  return UncheckedCast<Context>(cur_context.value());
}

TNode<Map> CodeStubAssembler::LoadJSArrayElementsMap(
    SloppyTNode<Int32T> kind, SloppyTNode<NativeContext> native_context) {
  CSA_ASSERT(this, IsFastElementsKind(kind));
  TNode<IntPtrT> offset =
      IntPtrAdd(IntPtrConstant(Context::FIRST_JS_ARRAY_MAP_SLOT),
                ChangeInt32ToIntPtr(kind));
  return UncheckedCast<Map>(LoadContextElement(native_context, offset));
}

TNode<Map> CodeStubAssembler::LoadJSArrayElementsMap(
    ElementsKind kind, SloppyTNode<NativeContext> native_context) {
  return UncheckedCast<Map>(
      LoadContextElement(native_context, Context::ArrayMapIndex(kind)));
}

TNode<BoolT> CodeStubAssembler::IsGeneratorFunction(
    TNode<JSFunction> function) {
  TNode<SharedFunctionInfo> const shared_function_info =
      LoadObjectField<SharedFunctionInfo>(
          function, JSFunction::kSharedFunctionInfoOffset);

  TNode<Uint32T> const function_kind =
      DecodeWord32<SharedFunctionInfo::FunctionKindBits>(LoadObjectField(
          shared_function_info, SharedFunctionInfo::kFlagsOffset,
          MachineType::Uint32()));

  return TNode<BoolT>::UncheckedCast(Word32Or(
      Word32Or(
          Word32Or(
              Word32Equal(function_kind,
                          Int32Constant(FunctionKind::kAsyncGeneratorFunction)),
              Word32Equal(
                  function_kind,
                  Int32Constant(FunctionKind::kAsyncConciseGeneratorMethod))),
          Word32Equal(function_kind,
                      Int32Constant(FunctionKind::kGeneratorFunction))),
      Word32Equal(function_kind,
                  Int32Constant(FunctionKind::kConciseGeneratorMethod))));
}

TNode<BoolT> CodeStubAssembler::HasPrototypeSlot(TNode<JSFunction> function) {
  return TNode<BoolT>::UncheckedCast(IsSetWord32<Map::HasPrototypeSlotBit>(
      LoadMapBitField(LoadMap(function))));
}

TNode<BoolT> CodeStubAssembler::HasPrototypeProperty(TNode<JSFunction> function,
                                                     TNode<Map> map) {
  // (has_prototype_slot() && IsConstructor()) ||
  // IsGeneratorFunction(shared()->kind())
  uint32_t mask =
      Map::HasPrototypeSlotBit::kMask | Map::IsConstructorBit::kMask;
  return TNode<BoolT>::UncheckedCast(
      Word32Or(IsAllSetWord32(LoadMapBitField(map), mask),
               IsGeneratorFunction(function)));
}

void CodeStubAssembler::GotoIfPrototypeRequiresRuntimeLookup(
    TNode<JSFunction> function, TNode<Map> map, Label* runtime) {
  // !has_prototype_property() || has_non_instance_prototype()
  GotoIfNot(HasPrototypeProperty(function, map), runtime);
  GotoIf(IsSetWord32<Map::HasNonInstancePrototypeBit>(LoadMapBitField(map)),
         runtime);
}

Node* CodeStubAssembler::LoadJSFunctionPrototype(TNode<JSFunction> function,
                                                 Label* if_bailout) {
  CSA_ASSERT(this, IsFunctionWithPrototypeSlotMap(LoadMap(function)));
  CSA_ASSERT(this, IsClearWord32<Map::HasNonInstancePrototypeBit>(
                       LoadMapBitField(LoadMap(function))));
  TNode<HeapObject> proto_or_map = LoadObjectField<HeapObject>(
      function, JSFunction::kPrototypeOrInitialMapOffset);
  GotoIf(IsTheHole(proto_or_map), if_bailout);

  TVARIABLE(HeapObject, var_result, proto_or_map);
  Label done(this, &var_result);
  GotoIfNot(IsMap(proto_or_map), &done);

  var_result = LoadMapPrototype(CAST(proto_or_map));
  Goto(&done);

  BIND(&done);
  return var_result.value();
}

TNode<BytecodeArray> CodeStubAssembler::LoadSharedFunctionInfoBytecodeArray(
    SloppyTNode<SharedFunctionInfo> shared) {
  TNode<Object> function_data =
      LoadObjectField(shared, SharedFunctionInfo::kFunctionDataOffset);

  VARIABLE(var_result, MachineRepresentation::kTagged, function_data);
  Label done(this, &var_result);

  GotoIfNot(HasInstanceType(CAST(function_data), INTERPRETER_DATA_TYPE), &done);
  TNode<Object> bytecode_array = LoadObjectField(
      CAST(function_data), InterpreterData::kBytecodeArrayOffset);
  var_result.Bind(bytecode_array);
  Goto(&done);

  BIND(&done);
  return CAST(var_result.value());
}

void CodeStubAssembler::StoreObjectByteNoWriteBarrier(TNode<HeapObject> object,
                                                      int offset,
                                                      TNode<Word32T> value) {
  StoreNoWriteBarrier(MachineRepresentation::kWord8, object,
                      IntPtrConstant(offset - kHeapObjectTag), value);
}

void CodeStubAssembler::StoreHeapNumberValue(SloppyTNode<HeapNumber> object,
                                             SloppyTNode<Float64T> value) {
  StoreObjectFieldNoWriteBarrier(object, HeapNumber::kValueOffset, value,
                                 MachineRepresentation::kFloat64);
}

void CodeStubAssembler::StoreObjectField(Node* object, int offset,
                                         Node* value) {
  DCHECK_NE(HeapObject::kMapOffset, offset);  // Use StoreMap instead.

  OptimizedStoreField(MachineRepresentation::kTagged,
                      UncheckedCast<HeapObject>(object), offset, value);
}

void CodeStubAssembler::StoreObjectField(Node* object, Node* offset,
                                         Node* value) {
  int const_offset;
  if (ToInt32Constant(offset, &const_offset)) {
    StoreObjectField(object, const_offset, value);
  } else {
    Store(object, IntPtrSub(offset, IntPtrConstant(kHeapObjectTag)), value);
  }
}

void CodeStubAssembler::StoreObjectFieldNoWriteBarrier(
    Node* object, int offset, Node* value, MachineRepresentation rep) {
  if (CanBeTaggedPointer(rep)) {
    OptimizedStoreFieldAssertNoWriteBarrier(
        rep, UncheckedCast<HeapObject>(object), offset, value);
  } else {
    OptimizedStoreFieldUnsafeNoWriteBarrier(
        rep, UncheckedCast<HeapObject>(object), offset, value);
  }
}

void CodeStubAssembler::UnsafeStoreObjectFieldNoWriteBarrier(
    TNode<HeapObject> object, int offset, TNode<Object> value) {
  OptimizedStoreFieldUnsafeNoWriteBarrier(MachineRepresentation::kTagged,
                                          object, offset, value);
}

void CodeStubAssembler::StoreObjectFieldNoWriteBarrier(
    Node* object, SloppyTNode<IntPtrT> offset, Node* value,
    MachineRepresentation rep) {
  int const_offset;
  if (ToInt32Constant(offset, &const_offset)) {
    return StoreObjectFieldNoWriteBarrier(object, const_offset, value, rep);
  }
  StoreNoWriteBarrier(rep, object,
                      IntPtrSub(offset, IntPtrConstant(kHeapObjectTag)), value);
}

void CodeStubAssembler::StoreMap(Node* object, Node* map) {
  OptimizedStoreMap(UncheckedCast<HeapObject>(object), CAST(map));
}

void CodeStubAssembler::StoreMapNoWriteBarrier(Node* object,
                                               RootIndex map_root_index) {
  StoreMapNoWriteBarrier(object, LoadRoot(map_root_index));
}

void CodeStubAssembler::StoreMapNoWriteBarrier(Node* object, Node* map) {
  CSA_SLOW_ASSERT(this, IsMap(map));
  OptimizedStoreFieldAssertNoWriteBarrier(MachineRepresentation::kTaggedPointer,
                                          UncheckedCast<HeapObject>(object),
                                          HeapObject::kMapOffset, map);
}

void CodeStubAssembler::StoreObjectFieldRoot(Node* object, int offset,
                                             RootIndex root_index) {
  if (RootsTable::IsImmortalImmovable(root_index)) {
    return StoreObjectFieldNoWriteBarrier(object, offset, LoadRoot(root_index));
  } else {
    return StoreObjectField(object, offset, LoadRoot(root_index));
  }
}

void CodeStubAssembler::StoreFixedArrayOrPropertyArrayElement(
    Node* object, Node* index_node, Node* value, WriteBarrierMode barrier_mode,
    int additional_offset, ParameterMode parameter_mode) {
  CSA_SLOW_ASSERT(
      this, Word32Or(IsFixedArraySubclass(object), IsPropertyArray(object)));
  CSA_SLOW_ASSERT(this, MatchesParameterMode(index_node, parameter_mode));
  DCHECK(barrier_mode == SKIP_WRITE_BARRIER ||
         barrier_mode == UNSAFE_SKIP_WRITE_BARRIER ||
         barrier_mode == UPDATE_WRITE_BARRIER ||
         barrier_mode == UPDATE_EPHEMERON_KEY_WRITE_BARRIER);
  DCHECK(IsAligned(additional_offset, kTaggedSize));
  STATIC_ASSERT(static_cast<int>(FixedArray::kHeaderSize) ==
                static_cast<int>(PropertyArray::kHeaderSize));
  int header_size =
      FixedArray::kHeaderSize + additional_offset - kHeapObjectTag;
  TNode<IntPtrT> offset = ElementOffsetFromIndex(index_node, HOLEY_ELEMENTS,
                                                 parameter_mode, header_size);
  STATIC_ASSERT(static_cast<int>(FixedArrayBase::kLengthOffset) ==
                static_cast<int>(WeakFixedArray::kLengthOffset));
  STATIC_ASSERT(static_cast<int>(FixedArrayBase::kLengthOffset) ==
                static_cast<int>(PropertyArray::kLengthAndHashOffset));
  // Check that index_node + additional_offset <= object.length.
  // TODO(cbruni): Use proper LoadXXLength helpers
  CSA_ASSERT(
      this,
      IsOffsetInBounds(
          offset,
          Select<IntPtrT>(
              IsPropertyArray(object),
              [=] {
                TNode<IntPtrT> length_and_hash = LoadAndUntagObjectField(
                    object, PropertyArray::kLengthAndHashOffset);
                return TNode<IntPtrT>::UncheckedCast(
                    DecodeWord<PropertyArray::LengthField>(length_and_hash));
              },
              [=] {
                return LoadAndUntagObjectField(object,
                                               FixedArrayBase::kLengthOffset);
              }),
          FixedArray::kHeaderSize));
  if (barrier_mode == SKIP_WRITE_BARRIER) {
    StoreNoWriteBarrier(MachineRepresentation::kTagged, object, offset, value);
  } else if (barrier_mode == UNSAFE_SKIP_WRITE_BARRIER) {
    UnsafeStoreNoWriteBarrier(MachineRepresentation::kTagged, object, offset,
                              value);
  } else if (barrier_mode == UPDATE_EPHEMERON_KEY_WRITE_BARRIER) {
    StoreEphemeronKey(object, offset, value);
  } else {
    Store(object, offset, value);
  }
}

void CodeStubAssembler::StoreFixedDoubleArrayElement(
    TNode<FixedDoubleArray> object, Node* index_node, TNode<Float64T> value,
    ParameterMode parameter_mode, CheckBounds check_bounds) {
  CSA_ASSERT(this, IsFixedDoubleArray(object));
  CSA_SLOW_ASSERT(this, MatchesParameterMode(index_node, parameter_mode));
  if (NeedsBoundsCheck(check_bounds)) {
    FixedArrayBoundsCheck(object, index_node, 0, parameter_mode);
  }
  TNode<IntPtrT> offset =
      ElementOffsetFromIndex(index_node, PACKED_DOUBLE_ELEMENTS, parameter_mode,
                             FixedArray::kHeaderSize - kHeapObjectTag);
  MachineRepresentation rep = MachineRepresentation::kFloat64;
  // Make sure we do not store signalling NaNs into double arrays.
  TNode<Float64T> value_silenced = Float64SilenceNaN(value);
  StoreNoWriteBarrier(rep, object, offset, value_silenced);
}

void CodeStubAssembler::StoreFeedbackVectorSlot(Node* object,
                                                Node* slot_index_node,
                                                Node* value,
                                                WriteBarrierMode barrier_mode,
                                                int additional_offset,
                                                ParameterMode parameter_mode) {
  CSA_SLOW_ASSERT(this, IsFeedbackVector(object));
  CSA_SLOW_ASSERT(this, MatchesParameterMode(slot_index_node, parameter_mode));
  DCHECK(IsAligned(additional_offset, kTaggedSize));
  DCHECK(barrier_mode == SKIP_WRITE_BARRIER ||
         barrier_mode == UNSAFE_SKIP_WRITE_BARRIER ||
         barrier_mode == UPDATE_WRITE_BARRIER);
  int header_size =
      FeedbackVector::kFeedbackSlotsOffset + additional_offset - kHeapObjectTag;
  TNode<IntPtrT> offset = ElementOffsetFromIndex(
      slot_index_node, HOLEY_ELEMENTS, parameter_mode, header_size);
  // Check that slot_index_node <= object.length.
  CSA_ASSERT(this,
             IsOffsetInBounds(offset, LoadFeedbackVectorLength(CAST(object)),
                              FeedbackVector::kHeaderSize));
  if (barrier_mode == SKIP_WRITE_BARRIER) {
    StoreNoWriteBarrier(MachineRepresentation::kTagged, object, offset, value);
  } else if (barrier_mode == UNSAFE_SKIP_WRITE_BARRIER) {
    UnsafeStoreNoWriteBarrier(MachineRepresentation::kTagged, object, offset,
                              value);
  } else {
    Store(object, offset, value);
  }
}

void CodeStubAssembler::EnsureArrayLengthWritable(TNode<Map> map,
                                                  Label* bailout) {
  // Don't support arrays in dictionary named property mode.
  GotoIf(IsDictionaryMap(map), bailout);

  // Check whether the length property is writable. The length property is the
  // only default named property on arrays. It's nonconfigurable, hence is
  // guaranteed to stay the first property.
  TNode<DescriptorArray> descriptors = LoadMapDescriptors(map);

  int length_index = JSArray::kLengthDescriptorIndex;
#ifdef DEBUG
  TNode<Name> maybe_length =
      LoadKeyByDescriptorEntry(descriptors, length_index);
  CSA_ASSERT(this, TaggedEqual(maybe_length, LengthStringConstant()));
#endif

  TNode<Uint32T> details =
      LoadDetailsByDescriptorEntry(descriptors, length_index);
  GotoIf(IsSetWord32(details, PropertyDetails::kAttributesReadOnlyMask),
         bailout);
}

TNode<Int32T> CodeStubAssembler::EnsureArrayPushable(TNode<Map> map,
                                                     Label* bailout) {
  // Disallow pushing onto prototypes. It might be the JSArray prototype.
  // Disallow pushing onto non-extensible objects.
  Comment("Disallow pushing onto prototypes");
  GotoIfNot(IsExtensibleNonPrototypeMap(map), bailout);

  EnsureArrayLengthWritable(map, bailout);

  TNode<Uint32T> kind =
      DecodeWord32<Map::ElementsKindBits>(LoadMapBitField2(map));
  return Signed(kind);
}

void CodeStubAssembler::PossiblyGrowElementsCapacity(
    ParameterMode mode, ElementsKind kind, Node* array, Node* length,
    Variable* var_elements, Node* growth, Label* bailout) {
  Label fits(this, var_elements);
  Node* capacity =
      TaggedToParameter(LoadFixedArrayBaseLength(var_elements->value()), mode);
  // length and growth nodes are already in a ParameterMode appropriate
  // representation.
  Node* new_length = IntPtrOrSmiAdd(growth, length, mode);
  GotoIfNot(IntPtrOrSmiGreaterThan(new_length, capacity, mode), &fits);
  Node* new_capacity = CalculateNewElementsCapacity(new_length, mode);
  var_elements->Bind(GrowElementsCapacity(array, var_elements->value(), kind,
                                          kind, capacity, new_capacity, mode,
                                          bailout));
  Goto(&fits);
  BIND(&fits);
}

TNode<Smi> CodeStubAssembler::BuildAppendJSArray(ElementsKind kind,
                                                 SloppyTNode<JSArray> array,
                                                 CodeStubArguments* args,
                                                 TVariable<IntPtrT>* arg_index,
                                                 Label* bailout) {
  CSA_SLOW_ASSERT(this, IsJSArray(array));
  Comment("BuildAppendJSArray: ", ElementsKindToString(kind));
  Label pre_bailout(this);
  Label success(this);
  TVARIABLE(Smi, var_tagged_length);
  ParameterMode mode = OptimalParameterMode();
  VARIABLE(var_length, OptimalParameterRepresentation(),
           TaggedToParameter(LoadFastJSArrayLength(array), mode));
  VARIABLE(var_elements, MachineRepresentation::kTagged, LoadElements(array));

  // Resize the capacity of the fixed array if it doesn't fit.
  TNode<IntPtrT> first = arg_index->value();
  Node* growth = IntPtrToParameter(
      IntPtrSub(UncheckedCast<IntPtrT>(args->GetLength(INTPTR_PARAMETERS)),
                first),
      mode);
  PossiblyGrowElementsCapacity(mode, kind, array, var_length.value(),
                               &var_elements, growth, &pre_bailout);

  // Push each argument onto the end of the array now that there is enough
  // capacity.
  CodeStubAssembler::VariableList push_vars({&var_length}, zone());
  Node* elements = var_elements.value();
  args->ForEach(
      push_vars,
      [this, kind, mode, elements, &var_length, &pre_bailout](Node* arg) {
        TryStoreArrayElement(kind, mode, &pre_bailout, elements,
                             var_length.value(), arg);
        Increment(&var_length, 1, mode);
      },
      first, nullptr);
  {
    TNode<Smi> length = ParameterToTagged(var_length.value(), mode);
    var_tagged_length = length;
    StoreObjectFieldNoWriteBarrier(array, JSArray::kLengthOffset, length);
    Goto(&success);
  }

  BIND(&pre_bailout);
  {
    TNode<Smi> length = ParameterToTagged(var_length.value(), mode);
    var_tagged_length = length;
    TNode<Smi> diff = SmiSub(length, LoadFastJSArrayLength(array));
    StoreObjectFieldNoWriteBarrier(array, JSArray::kLengthOffset, length);
    *arg_index = IntPtrAdd(arg_index->value(), SmiUntag(diff));
    Goto(bailout);
  }

  BIND(&success);
  return var_tagged_length.value();
}

void CodeStubAssembler::TryStoreArrayElement(ElementsKind kind,
                                             ParameterMode mode, Label* bailout,
                                             Node* elements, Node* index,
                                             Node* value) {
  if (IsSmiElementsKind(kind)) {
    GotoIf(TaggedIsNotSmi(value), bailout);
  } else if (IsDoubleElementsKind(kind)) {
    GotoIfNotNumber(value, bailout);
  }
  if (IsDoubleElementsKind(kind)) {
    value = ChangeNumberToFloat64(CAST(value));
  }
  StoreElement(elements, kind, index, value, mode);
}

void CodeStubAssembler::BuildAppendJSArray(ElementsKind kind, Node* array,
                                           Node* value, Label* bailout) {
  CSA_SLOW_ASSERT(this, IsJSArray(array));
  Comment("BuildAppendJSArray: ", ElementsKindToString(kind));
  ParameterMode mode = OptimalParameterMode();
  VARIABLE(var_length, OptimalParameterRepresentation(),
           TaggedToParameter(LoadFastJSArrayLength(array), mode));
  VARIABLE(var_elements, MachineRepresentation::kTagged, LoadElements(array));

  // Resize the capacity of the fixed array if it doesn't fit.
  Node* growth = IntPtrOrSmiConstant(1, mode);
  PossiblyGrowElementsCapacity(mode, kind, array, var_length.value(),
                               &var_elements, growth, bailout);

  // Push each argument onto the end of the array now that there is enough
  // capacity.
  TryStoreArrayElement(kind, mode, bailout, var_elements.value(),
                       var_length.value(), value);
  Increment(&var_length, 1, mode);

  TNode<Smi> length = ParameterToTagged(var_length.value(), mode);
  StoreObjectFieldNoWriteBarrier(array, JSArray::kLengthOffset, length);
}

Node* CodeStubAssembler::AllocateCellWithValue(Node* value,
                                               WriteBarrierMode mode) {
  TNode<HeapObject> result = Allocate(Cell::kSize, kNone);
  StoreMapNoWriteBarrier(result, RootIndex::kCellMap);
  StoreCellValue(result, value, mode);
  return result;
}

Node* CodeStubAssembler::LoadCellValue(Node* cell) {
  CSA_SLOW_ASSERT(this, HasInstanceType(cell, CELL_TYPE));
  return LoadObjectField(cell, Cell::kValueOffset);
}

void CodeStubAssembler::StoreCellValue(Node* cell, Node* value,
                                       WriteBarrierMode mode) {
  CSA_SLOW_ASSERT(this, HasInstanceType(cell, CELL_TYPE));
  DCHECK(mode == SKIP_WRITE_BARRIER || mode == UPDATE_WRITE_BARRIER);

  if (mode == UPDATE_WRITE_BARRIER) {
    StoreObjectField(cell, Cell::kValueOffset, value);
  } else {
    StoreObjectFieldNoWriteBarrier(cell, Cell::kValueOffset, value);
  }
}

TNode<HeapNumber> CodeStubAssembler::AllocateHeapNumber() {
  TNode<HeapObject> result = Allocate(HeapNumber::kSize, kNone);
  RootIndex heap_map_index = RootIndex::kHeapNumberMap;
  StoreMapNoWriteBarrier(result, heap_map_index);
  return UncheckedCast<HeapNumber>(result);
}

TNode<HeapNumber> CodeStubAssembler::AllocateHeapNumberWithValue(
    SloppyTNode<Float64T> value) {
  TNode<HeapNumber> result = AllocateHeapNumber();
  StoreHeapNumberValue(result, value);
  return result;
}

TNode<Object> CodeStubAssembler::CloneIfMutablePrimitive(TNode<Object> object) {
  TVARIABLE(Object, result, object);
  Label done(this);

  GotoIf(TaggedIsSmi(object), &done);
  // TODO(leszeks): Read the field descriptor to decide if this heap number is
  // mutable or not.
  GotoIfNot(IsHeapNumber(UncheckedCast<HeapObject>(object)), &done);
  {
    // Mutable heap number found --- allocate a clone.
    TNode<Float64T> value =
        LoadHeapNumberValue(UncheckedCast<HeapNumber>(object));
    result = AllocateHeapNumberWithValue(value);
    Goto(&done);
  }

  BIND(&done);
  return result.value();
}

TNode<BigInt> CodeStubAssembler::AllocateBigInt(TNode<IntPtrT> length) {
  TNode<BigInt> result = AllocateRawBigInt(length);
  StoreBigIntBitfield(result,
                      Word32Shl(TruncateIntPtrToInt32(length),
                                Int32Constant(BigInt::LengthBits::kShift)));
  return result;
}

TNode<BigInt> CodeStubAssembler::AllocateRawBigInt(TNode<IntPtrT> length) {
  TNode<IntPtrT> size =
      IntPtrAdd(IntPtrConstant(BigInt::kHeaderSize),
                Signed(WordShl(length, kSystemPointerSizeLog2)));
  TNode<HeapObject> raw_result = Allocate(size, kAllowLargeObjectAllocation);
  StoreMapNoWriteBarrier(raw_result, RootIndex::kBigIntMap);
  if (FIELD_SIZE(BigInt::kOptionalPaddingOffset) != 0) {
    DCHECK_EQ(4, FIELD_SIZE(BigInt::kOptionalPaddingOffset));
    StoreObjectFieldNoWriteBarrier(raw_result, BigInt::kOptionalPaddingOffset,
                                   Int32Constant(0),
                                   MachineRepresentation::kWord32);
  }
  return UncheckedCast<BigInt>(raw_result);
}

void CodeStubAssembler::StoreBigIntBitfield(TNode<BigInt> bigint,
                                            TNode<Word32T> bitfield) {
  StoreObjectFieldNoWriteBarrier(bigint, BigInt::kBitfieldOffset, bitfield,
                                 MachineRepresentation::kWord32);
}

void CodeStubAssembler::StoreBigIntDigit(TNode<BigInt> bigint,
                                         intptr_t digit_index,
                                         TNode<UintPtrT> digit) {
  CHECK_LE(0, digit_index);
  CHECK_LT(digit_index, BigInt::kMaxLength);
  StoreObjectFieldNoWriteBarrier(
      bigint,
      BigInt::kDigitsOffset +
          static_cast<int>(digit_index) * kSystemPointerSize,
      digit, UintPtrT::kMachineRepresentation);
}

void CodeStubAssembler::StoreBigIntDigit(TNode<BigInt> bigint,
                                         TNode<IntPtrT> digit_index,
                                         TNode<UintPtrT> digit) {
  TNode<IntPtrT> offset =
      IntPtrAdd(IntPtrConstant(BigInt::kDigitsOffset),
                IntPtrMul(digit_index, IntPtrConstant(kSystemPointerSize)));
  StoreObjectFieldNoWriteBarrier(bigint, offset, digit,
                                 UintPtrT::kMachineRepresentation);
}

TNode<Word32T> CodeStubAssembler::LoadBigIntBitfield(TNode<BigInt> bigint) {
  return UncheckedCast<Word32T>(
      LoadObjectField(bigint, BigInt::kBitfieldOffset, MachineType::Uint32()));
}

TNode<UintPtrT> CodeStubAssembler::LoadBigIntDigit(TNode<BigInt> bigint,
                                                   intptr_t digit_index) {
  CHECK_LE(0, digit_index);
  CHECK_LT(digit_index, BigInt::kMaxLength);
  return UncheckedCast<UintPtrT>(
      LoadObjectField(bigint,
                      BigInt::kDigitsOffset +
                          static_cast<int>(digit_index) * kSystemPointerSize,
                      MachineType::UintPtr()));
}

TNode<UintPtrT> CodeStubAssembler::LoadBigIntDigit(TNode<BigInt> bigint,
                                                   TNode<IntPtrT> digit_index) {
  TNode<IntPtrT> offset =
      IntPtrAdd(IntPtrConstant(BigInt::kDigitsOffset),
                IntPtrMul(digit_index, IntPtrConstant(kSystemPointerSize)));
  return UncheckedCast<UintPtrT>(
      LoadObjectField(bigint, offset, MachineType::UintPtr()));
}

TNode<ByteArray> CodeStubAssembler::AllocateByteArray(TNode<UintPtrT> length,
                                                      AllocationFlags flags) {
  Comment("AllocateByteArray");
  VARIABLE(var_result, MachineRepresentation::kTagged);

  // Compute the ByteArray size and check if it fits into new space.
  Label if_lengthiszero(this), if_sizeissmall(this),
      if_notsizeissmall(this, Label::kDeferred), if_join(this);
  GotoIf(WordEqual(length, UintPtrConstant(0)), &if_lengthiszero);

  TNode<IntPtrT> raw_size =
      GetArrayAllocationSize(Signed(length), UINT8_ELEMENTS, INTPTR_PARAMETERS,
                             ByteArray::kHeaderSize + kObjectAlignmentMask);
  TNode<WordT> size = WordAnd(raw_size, IntPtrConstant(~kObjectAlignmentMask));
  Branch(IntPtrLessThanOrEqual(size, IntPtrConstant(kMaxRegularHeapObjectSize)),
         &if_sizeissmall, &if_notsizeissmall);

  BIND(&if_sizeissmall);
  {
    // Just allocate the ByteArray in new space.
    TNode<HeapObject> result =
        AllocateInNewSpace(UncheckedCast<IntPtrT>(size), flags);
    DCHECK(RootsTable::IsImmortalImmovable(RootIndex::kByteArrayMap));
    StoreMapNoWriteBarrier(result, RootIndex::kByteArrayMap);
    StoreObjectFieldNoWriteBarrier(result, ByteArray::kLengthOffset,
                                   SmiTag(Signed(length)));
    var_result.Bind(result);
    Goto(&if_join);
  }

  BIND(&if_notsizeissmall);
  {
    // We might need to allocate in large object space, go to the runtime.
    TNode<Object> result =
        CallRuntime(Runtime::kAllocateByteArray, NoContextConstant(),
                    ChangeUintPtrToTagged(length));
    var_result.Bind(result);
    Goto(&if_join);
  }

  BIND(&if_lengthiszero);
  {
    var_result.Bind(EmptyByteArrayConstant());
    Goto(&if_join);
  }

  BIND(&if_join);
  return CAST(var_result.value());
}

TNode<String> CodeStubAssembler::AllocateSeqOneByteString(
    uint32_t length, AllocationFlags flags) {
  Comment("AllocateSeqOneByteString");
  if (length == 0) {
    return EmptyStringConstant();
  }
  TNode<HeapObject> result = Allocate(SeqOneByteString::SizeFor(length), flags);
  DCHECK(RootsTable::IsImmortalImmovable(RootIndex::kOneByteStringMap));
  StoreMapNoWriteBarrier(result, RootIndex::kOneByteStringMap);
  StoreObjectFieldNoWriteBarrier(result, SeqOneByteString::kLengthOffset,
                                 Uint32Constant(length),
                                 MachineRepresentation::kWord32);
  StoreObjectFieldNoWriteBarrier(result, SeqOneByteString::kHashFieldOffset,
                                 Int32Constant(String::kEmptyHashField),
                                 MachineRepresentation::kWord32);
  return CAST(result);
}

TNode<BoolT> CodeStubAssembler::IsZeroOrContext(SloppyTNode<Object> object) {
  return Select<BoolT>(
      TaggedEqual(object, SmiConstant(0)), [=] { return Int32TrueConstant(); },
      [=] { return IsContext(CAST(object)); });
}

TNode<String> CodeStubAssembler::AllocateSeqOneByteString(
    TNode<Uint32T> length, AllocationFlags flags) {
  Comment("AllocateSeqOneByteString");
  VARIABLE(var_result, MachineRepresentation::kTagged);

  // Compute the SeqOneByteString size and check if it fits into new space.
  Label if_lengthiszero(this), if_sizeissmall(this),
      if_notsizeissmall(this, Label::kDeferred), if_join(this);
  GotoIf(Word32Equal(length, Uint32Constant(0)), &if_lengthiszero);

  TNode<IntPtrT> raw_size = GetArrayAllocationSize(
      Signed(ChangeUint32ToWord(length)), UINT8_ELEMENTS, INTPTR_PARAMETERS,
      SeqOneByteString::kHeaderSize + kObjectAlignmentMask);
  TNode<WordT> size = WordAnd(raw_size, IntPtrConstant(~kObjectAlignmentMask));
  Branch(IntPtrLessThanOrEqual(size, IntPtrConstant(kMaxRegularHeapObjectSize)),
         &if_sizeissmall, &if_notsizeissmall);

  BIND(&if_sizeissmall);
  {
    // Just allocate the SeqOneByteString in new space.
    TNode<HeapObject> result =
        AllocateInNewSpace(UncheckedCast<IntPtrT>(size), flags);
    DCHECK(RootsTable::IsImmortalImmovable(RootIndex::kOneByteStringMap));
    StoreMapNoWriteBarrier(result, RootIndex::kOneByteStringMap);
    StoreObjectFieldNoWriteBarrier(result, SeqOneByteString::kLengthOffset,
                                   length, MachineRepresentation::kWord32);
    StoreObjectFieldNoWriteBarrier(result, SeqOneByteString::kHashFieldOffset,
                                   Int32Constant(String::kEmptyHashField),
                                   MachineRepresentation::kWord32);
    var_result.Bind(result);
    Goto(&if_join);
  }

  BIND(&if_notsizeissmall);
  {
    // We might need to allocate in large object space, go to the runtime.
    TNode<Object> result =
        CallRuntime(Runtime::kAllocateSeqOneByteString, NoContextConstant(),
                    ChangeUint32ToTagged(length));
    var_result.Bind(result);
    Goto(&if_join);
  }

  BIND(&if_lengthiszero);
  {
    var_result.Bind(EmptyStringConstant());
    Goto(&if_join);
  }

  BIND(&if_join);
  return CAST(var_result.value());
}

TNode<String> CodeStubAssembler::AllocateSeqTwoByteString(
    uint32_t length, AllocationFlags flags) {
  Comment("AllocateSeqTwoByteString");
  if (length == 0) {
    return EmptyStringConstant();
  }
  TNode<HeapObject> result = Allocate(SeqTwoByteString::SizeFor(length), flags);
  DCHECK(RootsTable::IsImmortalImmovable(RootIndex::kStringMap));
  StoreMapNoWriteBarrier(result, RootIndex::kStringMap);
  StoreObjectFieldNoWriteBarrier(result, SeqTwoByteString::kLengthOffset,
                                 Uint32Constant(length),
                                 MachineRepresentation::kWord32);
  StoreObjectFieldNoWriteBarrier(result, SeqTwoByteString::kHashFieldOffset,
                                 Int32Constant(String::kEmptyHashField),
                                 MachineRepresentation::kWord32);
  return CAST(result);
}

TNode<String> CodeStubAssembler::AllocateSeqTwoByteString(
    TNode<Uint32T> length, AllocationFlags flags) {
  Comment("AllocateSeqTwoByteString");
  VARIABLE(var_result, MachineRepresentation::kTagged);

  // Compute the SeqTwoByteString size and check if it fits into new space.
  Label if_lengthiszero(this), if_sizeissmall(this),
      if_notsizeissmall(this, Label::kDeferred), if_join(this);
  GotoIf(Word32Equal(length, Uint32Constant(0)), &if_lengthiszero);

  TNode<IntPtrT> raw_size = GetArrayAllocationSize(
      Signed(ChangeUint32ToWord(length)), UINT16_ELEMENTS, INTPTR_PARAMETERS,
      SeqOneByteString::kHeaderSize + kObjectAlignmentMask);
  TNode<WordT> size = WordAnd(raw_size, IntPtrConstant(~kObjectAlignmentMask));
  Branch(IntPtrLessThanOrEqual(size, IntPtrConstant(kMaxRegularHeapObjectSize)),
         &if_sizeissmall, &if_notsizeissmall);

  BIND(&if_sizeissmall);
  {
    // Just allocate the SeqTwoByteString in new space.
    TNode<HeapObject> result =
        AllocateInNewSpace(UncheckedCast<IntPtrT>(size), flags);
    DCHECK(RootsTable::IsImmortalImmovable(RootIndex::kStringMap));
    StoreMapNoWriteBarrier(result, RootIndex::kStringMap);
    StoreObjectFieldNoWriteBarrier(result, SeqTwoByteString::kLengthOffset,
                                   length, MachineRepresentation::kWord32);
    StoreObjectFieldNoWriteBarrier(result, SeqTwoByteString::kHashFieldOffset,
                                   Int32Constant(String::kEmptyHashField),
                                   MachineRepresentation::kWord32);
    var_result.Bind(result);
    Goto(&if_join);
  }

  BIND(&if_notsizeissmall);
  {
    // We might need to allocate in large object space, go to the runtime.
    TNode<Object> result =
        CallRuntime(Runtime::kAllocateSeqTwoByteString, NoContextConstant(),
                    ChangeUint32ToTagged(length));
    var_result.Bind(result);
    Goto(&if_join);
  }

  BIND(&if_lengthiszero);
  {
    var_result.Bind(EmptyStringConstant());
    Goto(&if_join);
  }

  BIND(&if_join);
  return CAST(var_result.value());
}

TNode<String> CodeStubAssembler::AllocateSlicedString(RootIndex map_root_index,
                                                      TNode<Uint32T> length,
                                                      TNode<String> parent,
                                                      TNode<Smi> offset) {
  DCHECK(map_root_index == RootIndex::kSlicedOneByteStringMap ||
         map_root_index == RootIndex::kSlicedStringMap);
  TNode<HeapObject> result = Allocate(SlicedString::kSize);
  DCHECK(RootsTable::IsImmortalImmovable(map_root_index));
  StoreMapNoWriteBarrier(result, map_root_index);
  StoreObjectFieldNoWriteBarrier(result, SlicedString::kHashFieldOffset,
                                 Int32Constant(String::kEmptyHashField),
                                 MachineRepresentation::kWord32);
  StoreObjectFieldNoWriteBarrier(result, SlicedString::kLengthOffset, length,
                                 MachineRepresentation::kWord32);
  StoreObjectFieldNoWriteBarrier(result, SlicedString::kParentOffset, parent,
                                 MachineRepresentation::kTagged);
  StoreObjectFieldNoWriteBarrier(result, SlicedString::kOffsetOffset, offset,
                                 MachineRepresentation::kTagged);
  return CAST(result);
}

TNode<String> CodeStubAssembler::AllocateSlicedOneByteString(
    TNode<Uint32T> length, TNode<String> parent, TNode<Smi> offset) {
  return AllocateSlicedString(RootIndex::kSlicedOneByteStringMap, length,
                              parent, offset);
}

TNode<String> CodeStubAssembler::AllocateSlicedTwoByteString(
    TNode<Uint32T> length, TNode<String> parent, TNode<Smi> offset) {
  return AllocateSlicedString(RootIndex::kSlicedStringMap, length, parent,
                              offset);
}

TNode<String> CodeStubAssembler::AllocateConsString(TNode<Uint32T> length,
                                                    TNode<String> left,
                                                    TNode<String> right) {
  // Added string can be a cons string.
  Comment("Allocating ConsString");
  TNode<Int32T> left_instance_type = LoadInstanceType(left);
  TNode<Int32T> right_instance_type = LoadInstanceType(right);

  // Determine the resulting ConsString map to use depending on whether
  // any of {left} or {right} has two byte encoding.
  STATIC_ASSERT(kOneByteStringTag != 0);
  STATIC_ASSERT(kTwoByteStringTag == 0);
  TNode<Int32T> combined_instance_type =
      Word32And(left_instance_type, right_instance_type);
  TNode<Map> result_map = CAST(Select<Object>(
      IsSetWord32(combined_instance_type, kStringEncodingMask),
      [=] { return ConsOneByteStringMapConstant(); },
      [=] { return ConsStringMapConstant(); }));
  TNode<HeapObject> result = AllocateInNewSpace(ConsString::kSize);
  StoreMapNoWriteBarrier(result, result_map);
  StoreObjectFieldNoWriteBarrier(result, ConsString::kLengthOffset, length,
                                 MachineRepresentation::kWord32);
  StoreObjectFieldNoWriteBarrier(result, ConsString::kHashFieldOffset,
                                 Int32Constant(String::kEmptyHashField),
                                 MachineRepresentation::kWord32);
  StoreObjectFieldNoWriteBarrier(result, ConsString::kFirstOffset, left);
  StoreObjectFieldNoWriteBarrier(result, ConsString::kSecondOffset, right);
  return CAST(result);
}

TNode<NameDictionary> CodeStubAssembler::AllocateNameDictionary(
    int at_least_space_for) {
  return AllocateNameDictionary(IntPtrConstant(at_least_space_for));
}

TNode<NameDictionary> CodeStubAssembler::AllocateNameDictionary(
    TNode<IntPtrT> at_least_space_for, AllocationFlags flags) {
  CSA_ASSERT(this, UintPtrLessThanOrEqual(
                       at_least_space_for,
                       IntPtrConstant(NameDictionary::kMaxCapacity)));
  TNode<IntPtrT> capacity = HashTableComputeCapacity(at_least_space_for);
  return AllocateNameDictionaryWithCapacity(capacity, flags);
}

TNode<NameDictionary> CodeStubAssembler::AllocateNameDictionaryWithCapacity(
    TNode<IntPtrT> capacity, AllocationFlags flags) {
  CSA_ASSERT(this, WordIsPowerOfTwo(capacity));
  CSA_ASSERT(this, IntPtrGreaterThan(capacity, IntPtrConstant(0)));
  TNode<IntPtrT> length = EntryToIndex<NameDictionary>(capacity);
  TNode<IntPtrT> store_size = IntPtrAdd(
      TimesTaggedSize(length), IntPtrConstant(NameDictionary::kHeaderSize));

  TNode<NameDictionary> result =
      UncheckedCast<NameDictionary>(Allocate(store_size, flags));

  // Initialize FixedArray fields.
  {
    DCHECK(RootsTable::IsImmortalImmovable(RootIndex::kNameDictionaryMap));
    StoreMapNoWriteBarrier(result, RootIndex::kNameDictionaryMap);
    StoreObjectFieldNoWriteBarrier(result, FixedArray::kLengthOffset,
                                   SmiFromIntPtr(length));
  }

  // Initialized HashTable fields.
  {
    TNode<Smi> zero = SmiConstant(0);
    StoreFixedArrayElement(result, NameDictionary::kNumberOfElementsIndex, zero,
                           SKIP_WRITE_BARRIER);
    StoreFixedArrayElement(result,
                           NameDictionary::kNumberOfDeletedElementsIndex, zero,
                           SKIP_WRITE_BARRIER);
    StoreFixedArrayElement(result, NameDictionary::kCapacityIndex,
                           SmiTag(capacity), SKIP_WRITE_BARRIER);
    // Initialize Dictionary fields.
    StoreFixedArrayElement(result, NameDictionary::kNextEnumerationIndexIndex,
                           SmiConstant(PropertyDetails::kInitialIndex),
                           SKIP_WRITE_BARRIER);
    StoreFixedArrayElement(result, NameDictionary::kObjectHashIndex,
                           SmiConstant(PropertyArray::kNoHashSentinel),
                           SKIP_WRITE_BARRIER);
  }

  // Initialize NameDictionary elements.
  {
    TNode<IntPtrT> result_word = BitcastTaggedToWord(result);
    TNode<IntPtrT> start_address = IntPtrAdd(
        result_word, IntPtrConstant(NameDictionary::OffsetOfElementAt(
                                        NameDictionary::kElementsStartIndex) -
                                    kHeapObjectTag));
    TNode<IntPtrT> end_address = IntPtrAdd(
        result_word, IntPtrSub(store_size, IntPtrConstant(kHeapObjectTag)));

    TNode<Oddball> filler = UndefinedConstant();
    DCHECK(RootsTable::IsImmortalImmovable(RootIndex::kUndefinedValue));

    StoreFieldsNoWriteBarrier(start_address, end_address, filler);
  }

  return result;
}

TNode<NameDictionary> CodeStubAssembler::CopyNameDictionary(
    TNode<NameDictionary> dictionary, Label* large_object_fallback) {
  Comment("Copy boilerplate property dict");
  TNode<IntPtrT> capacity = SmiUntag(GetCapacity<NameDictionary>(dictionary));
  CSA_ASSERT(this, IntPtrGreaterThanOrEqual(capacity, IntPtrConstant(0)));
  GotoIf(UintPtrGreaterThan(
             capacity, IntPtrConstant(NameDictionary::kMaxRegularCapacity)),
         large_object_fallback);
  TNode<NameDictionary> properties =
      AllocateNameDictionaryWithCapacity(capacity);
  TNode<IntPtrT> length = SmiUntag(LoadFixedArrayBaseLength(dictionary));
  CopyFixedArrayElements(PACKED_ELEMENTS, dictionary, properties, length,
                         SKIP_WRITE_BARRIER, INTPTR_PARAMETERS);
  return properties;
}

template <typename CollectionType>
Node* CodeStubAssembler::AllocateOrderedHashTable() {
  static const int kCapacity = CollectionType::kMinCapacity;
  static const int kBucketCount = kCapacity / CollectionType::kLoadFactor;
  static const int kDataTableLength = kCapacity * CollectionType::kEntrySize;
  static const int kFixedArrayLength =
      CollectionType::HashTableStartIndex() + kBucketCount + kDataTableLength;
  static const int kDataTableStartIndex =
      CollectionType::HashTableStartIndex() + kBucketCount;

  STATIC_ASSERT(base::bits::IsPowerOfTwo(kCapacity));
  STATIC_ASSERT(kCapacity <= CollectionType::MaxCapacity());

  // Allocate the table and add the proper map.
  const ElementsKind elements_kind = HOLEY_ELEMENTS;
  TNode<IntPtrT> length_intptr = IntPtrConstant(kFixedArrayLength);
  TNode<Map> fixed_array_map =
      CAST(LoadRoot(CollectionType::GetMapRootIndex()));
  TNode<FixedArray> table =
      CAST(AllocateFixedArray(elements_kind, length_intptr,
                              kAllowLargeObjectAllocation, fixed_array_map));

  // Initialize the OrderedHashTable fields.
  const WriteBarrierMode barrier_mode = SKIP_WRITE_BARRIER;
  StoreFixedArrayElement(table, CollectionType::NumberOfElementsIndex(),
                         SmiConstant(0), barrier_mode);
  StoreFixedArrayElement(table, CollectionType::NumberOfDeletedElementsIndex(),
                         SmiConstant(0), barrier_mode);
  StoreFixedArrayElement(table, CollectionType::NumberOfBucketsIndex(),
                         SmiConstant(kBucketCount), barrier_mode);

  // Fill the buckets with kNotFound.
  TNode<Smi> not_found = SmiConstant(CollectionType::kNotFound);
  STATIC_ASSERT(CollectionType::HashTableStartIndex() ==
                CollectionType::NumberOfBucketsIndex() + 1);
  STATIC_ASSERT((CollectionType::HashTableStartIndex() + kBucketCount) ==
                kDataTableStartIndex);
  for (int i = 0; i < kBucketCount; i++) {
    StoreFixedArrayElement(table, CollectionType::HashTableStartIndex() + i,
                           not_found, barrier_mode);
  }

  // Fill the data table with undefined.
  STATIC_ASSERT(kDataTableStartIndex + kDataTableLength == kFixedArrayLength);
  for (int i = 0; i < kDataTableLength; i++) {
    StoreFixedArrayElement(table, kDataTableStartIndex + i, UndefinedConstant(),
                           barrier_mode);
  }

  return table;
}

template Node* CodeStubAssembler::AllocateOrderedHashTable<OrderedHashMap>();
template Node* CodeStubAssembler::AllocateOrderedHashTable<OrderedHashSet>();

template <typename CollectionType>
TNode<CollectionType> CodeStubAssembler::AllocateSmallOrderedHashTable(
    TNode<IntPtrT> capacity) {
  CSA_ASSERT(this, WordIsPowerOfTwo(capacity));
  CSA_ASSERT(this, IntPtrLessThan(
                       capacity, IntPtrConstant(CollectionType::kMaxCapacity)));

  TNode<IntPtrT> data_table_start_offset =
      IntPtrConstant(CollectionType::DataTableStartOffset());

  TNode<IntPtrT> data_table_size = IntPtrMul(
      capacity, IntPtrConstant(CollectionType::kEntrySize * kTaggedSize));

  TNode<Int32T> hash_table_size =
      Int32Div(TruncateIntPtrToInt32(capacity),
               Int32Constant(CollectionType::kLoadFactor));

  TNode<IntPtrT> hash_table_start_offset =
      IntPtrAdd(data_table_start_offset, data_table_size);

  TNode<IntPtrT> hash_table_and_chain_table_size =
      IntPtrAdd(ChangeInt32ToIntPtr(hash_table_size), capacity);

  TNode<IntPtrT> total_size =
      IntPtrAdd(hash_table_start_offset, hash_table_and_chain_table_size);

  TNode<IntPtrT> total_size_word_aligned =
      IntPtrAdd(total_size, IntPtrConstant(kTaggedSize - 1));
  total_size_word_aligned = ChangeInt32ToIntPtr(
      Int32Div(TruncateIntPtrToInt32(total_size_word_aligned),
               Int32Constant(kTaggedSize)));
  total_size_word_aligned =
      UncheckedCast<IntPtrT>(TimesTaggedSize(total_size_word_aligned));

  // Allocate the table and add the proper map.
  TNode<Map> small_ordered_hash_map =
      CAST(LoadRoot(CollectionType::GetMapRootIndex()));
  TNode<HeapObject> table_obj = AllocateInNewSpace(total_size_word_aligned);
  StoreMapNoWriteBarrier(table_obj, small_ordered_hash_map);
  TNode<CollectionType> table = UncheckedCast<CollectionType>(table_obj);

  {
    // This store overlaps with the header fields stored below.
    // Since it happens first, it effectively still just zero-initializes the
    // padding.
    constexpr int offset =
        RoundDown<kTaggedSize>(CollectionType::PaddingOffset());
    STATIC_ASSERT(offset + kTaggedSize == CollectionType::PaddingOffset() +
                                              CollectionType::PaddingSize());
    StoreObjectFieldNoWriteBarrier(table, offset, SmiConstant(0));
  }

  // Initialize the SmallOrderedHashTable fields.
  StoreObjectByteNoWriteBarrier(
      table, CollectionType::NumberOfBucketsOffset(),
      Word32And(Int32Constant(0xFF), hash_table_size));
  StoreObjectByteNoWriteBarrier(table, CollectionType::NumberOfElementsOffset(),
                                Int32Constant(0));
  StoreObjectByteNoWriteBarrier(
      table, CollectionType::NumberOfDeletedElementsOffset(), Int32Constant(0));

  TNode<IntPtrT> table_address =
      IntPtrSub(BitcastTaggedToWord(table), IntPtrConstant(kHeapObjectTag));
  TNode<IntPtrT> hash_table_start_address =
      IntPtrAdd(table_address, hash_table_start_offset);

  // Initialize the HashTable part.
  TNode<ExternalReference> memset =
      ExternalConstant(ExternalReference::libc_memset_function());
  CallCFunction(
      memset, MachineType::AnyTagged(),
      std::make_pair(MachineType::Pointer(), hash_table_start_address),
      std::make_pair(MachineType::IntPtr(), IntPtrConstant(0xFF)),
      std::make_pair(MachineType::UintPtr(), hash_table_and_chain_table_size));

  // Initialize the DataTable part.
  TNode<Oddball> filler = TheHoleConstant();
  TNode<IntPtrT> data_table_start_address =
      IntPtrAdd(table_address, data_table_start_offset);
  TNode<IntPtrT> data_table_end_address =
      IntPtrAdd(data_table_start_address, data_table_size);
  StoreFieldsNoWriteBarrier(data_table_start_address, data_table_end_address,
                            filler);

  return table;
}

template V8_EXPORT_PRIVATE TNode<SmallOrderedHashMap>
CodeStubAssembler::AllocateSmallOrderedHashTable<SmallOrderedHashMap>(
    TNode<IntPtrT> capacity);
template V8_EXPORT_PRIVATE TNode<SmallOrderedHashSet>
CodeStubAssembler::AllocateSmallOrderedHashTable<SmallOrderedHashSet>(
    TNode<IntPtrT> capacity);

template <typename CollectionType>
void CodeStubAssembler::FindOrderedHashTableEntry(
    Node* table, Node* hash,
    const std::function<void(TNode<Object>, Label*, Label*)>& key_compare,
    Variable* entry_start_position, Label* entry_found, Label* not_found) {
  // Get the index of the bucket.
  TNode<IntPtrT> const number_of_buckets =
      SmiUntag(CAST(UnsafeLoadFixedArrayElement(
          CAST(table), CollectionType::NumberOfBucketsIndex())));
  TNode<WordT> const bucket =
      WordAnd(hash, IntPtrSub(number_of_buckets, IntPtrConstant(1)));
  TNode<IntPtrT> const first_entry = SmiUntag(CAST(UnsafeLoadFixedArrayElement(
      CAST(table), bucket,
      CollectionType::HashTableStartIndex() * kTaggedSize)));

  // Walk the bucket chain.
  TNode<IntPtrT> entry_start;
  Label if_key_found(this);
  {
    TVARIABLE(IntPtrT, var_entry, first_entry);
    Label loop(this, {&var_entry, entry_start_position}),
        continue_next_entry(this);
    Goto(&loop);
    BIND(&loop);

    // If the entry index is the not-found sentinel, we are done.
    GotoIf(IntPtrEqual(var_entry.value(),
                       IntPtrConstant(CollectionType::kNotFound)),
           not_found);

    // Make sure the entry index is within range.
    CSA_ASSERT(
        this,
        UintPtrLessThan(
            var_entry.value(),
            SmiUntag(SmiAdd(
                CAST(UnsafeLoadFixedArrayElement(
                    CAST(table), CollectionType::NumberOfElementsIndex())),
                CAST(UnsafeLoadFixedArrayElement(
                    CAST(table),
                    CollectionType::NumberOfDeletedElementsIndex()))))));

    // Compute the index of the entry relative to kHashTableStartIndex.
    entry_start =
        IntPtrAdd(IntPtrMul(var_entry.value(),
                            IntPtrConstant(CollectionType::kEntrySize)),
                  number_of_buckets);

    // Load the key from the entry.
    TNode<Object> const candidate_key = UnsafeLoadFixedArrayElement(
        CAST(table), entry_start,
        CollectionType::HashTableStartIndex() * kTaggedSize);

    key_compare(candidate_key, &if_key_found, &continue_next_entry);

    BIND(&continue_next_entry);
    // Load the index of the next entry in the bucket chain.
    var_entry = SmiUntag(CAST(UnsafeLoadFixedArrayElement(
        CAST(table), entry_start,
        (CollectionType::HashTableStartIndex() + CollectionType::kChainOffset) *
            kTaggedSize)));

    Goto(&loop);
  }

  BIND(&if_key_found);
  entry_start_position->Bind(entry_start);
  Goto(entry_found);
}

template void CodeStubAssembler::FindOrderedHashTableEntry<OrderedHashMap>(
    Node* table, Node* hash,
    const std::function<void(TNode<Object>, Label*, Label*)>& key_compare,
    Variable* entry_start_position, Label* entry_found, Label* not_found);
template void CodeStubAssembler::FindOrderedHashTableEntry<OrderedHashSet>(
    Node* table, Node* hash,
    const std::function<void(TNode<Object>, Label*, Label*)>& key_compare,
    Variable* entry_start_position, Label* entry_found, Label* not_found);

Node* CodeStubAssembler::AllocateStruct(Node* map, AllocationFlags flags) {
  Comment("AllocateStruct");
  CSA_ASSERT(this, IsMap(map));
  TNode<IntPtrT> size = TimesTaggedSize(LoadMapInstanceSizeInWords(map));
  TNode<HeapObject> object = Allocate(size, flags);
  StoreMapNoWriteBarrier(object, map);
  InitializeStructBody(object, map, size, Struct::kHeaderSize);
  return object;
}

void CodeStubAssembler::InitializeStructBody(Node* object, Node* map,
                                             Node* size, int start_offset) {
  CSA_SLOW_ASSERT(this, IsMap(map));
  Comment("InitializeStructBody");
  TNode<Oddball> filler = UndefinedConstant();
  // Calculate the untagged field addresses.
  object = BitcastTaggedToWord(object);
  TNode<WordT> start_address =
      IntPtrAdd(object, IntPtrConstant(start_offset - kHeapObjectTag));
  TNode<WordT> end_address =
      IntPtrSub(IntPtrAdd(object, size), IntPtrConstant(kHeapObjectTag));
  StoreFieldsNoWriteBarrier(start_address, end_address, filler);
}

TNode<JSObject> CodeStubAssembler::AllocateJSObjectFromMap(
    SloppyTNode<Map> map, SloppyTNode<HeapObject> properties,
    SloppyTNode<FixedArray> elements, AllocationFlags flags,
    SlackTrackingMode slack_tracking_mode) {
  CSA_ASSERT(this, IsMap(map));
  CSA_ASSERT(this, Word32BinaryNot(IsJSFunctionMap(map)));
  CSA_ASSERT(this, Word32BinaryNot(InstanceTypeEqual(LoadMapInstanceType(map),
                                                     JS_GLOBAL_OBJECT_TYPE)));
  TNode<IntPtrT> instance_size =
      TimesTaggedSize(LoadMapInstanceSizeInWords(map));
  TNode<HeapObject> object = AllocateInNewSpace(instance_size, flags);
  StoreMapNoWriteBarrier(object, map);
  InitializeJSObjectFromMap(object, map, instance_size, properties, elements,
                            slack_tracking_mode);
  return CAST(object);
}

void CodeStubAssembler::InitializeJSObjectFromMap(
    Node* object, Node* map, Node* instance_size, Node* properties,
    Node* elements, SlackTrackingMode slack_tracking_mode) {
  CSA_SLOW_ASSERT(this, IsMap(map));
  // This helper assumes that the object is in new-space, as guarded by the
  // check in AllocatedJSObjectFromMap.
  if (properties == nullptr) {
    CSA_ASSERT(this, Word32BinaryNot(IsDictionaryMap((map))));
    StoreObjectFieldRoot(object, JSObject::kPropertiesOrHashOffset,
                         RootIndex::kEmptyFixedArray);
  } else {
    CSA_ASSERT(this, Word32Or(Word32Or(IsPropertyArray(properties),
                                       IsNameDictionary(properties)),
                              IsEmptyFixedArray(properties)));
    StoreObjectFieldNoWriteBarrier(object, JSObject::kPropertiesOrHashOffset,
                                   properties);
  }
  if (elements == nullptr) {
    StoreObjectFieldRoot(object, JSObject::kElementsOffset,
                         RootIndex::kEmptyFixedArray);
  } else {
    CSA_ASSERT(this, IsFixedArray(elements));
    StoreObjectFieldNoWriteBarrier(object, JSObject::kElementsOffset, elements);
  }
  if (slack_tracking_mode == kNoSlackTracking) {
    InitializeJSObjectBodyNoSlackTracking(object, map, instance_size);
  } else {
    DCHECK_EQ(slack_tracking_mode, kWithSlackTracking);
    InitializeJSObjectBodyWithSlackTracking(object, map, instance_size);
  }
}

void CodeStubAssembler::InitializeJSObjectBodyNoSlackTracking(
    Node* object, Node* map, Node* instance_size, int start_offset) {
  STATIC_ASSERT(Map::kNoSlackTracking == 0);
  CSA_ASSERT(
      this, IsClearWord32<Map::ConstructionCounterBits>(LoadMapBitField3(map)));
  InitializeFieldsWithRoot(object, IntPtrConstant(start_offset), instance_size,
                           RootIndex::kUndefinedValue);
}

void CodeStubAssembler::InitializeJSObjectBodyWithSlackTracking(
    Node* object, Node* map, Node* instance_size) {
  CSA_SLOW_ASSERT(this, IsMap(map));
  Comment("InitializeJSObjectBodyNoSlackTracking");

  // Perform in-object slack tracking if requested.
  int start_offset = JSObject::kHeaderSize;
  TNode<Uint32T> bit_field3 = LoadMapBitField3(map);
  Label end(this), slack_tracking(this), complete(this, Label::kDeferred);
  STATIC_ASSERT(Map::kNoSlackTracking == 0);
  GotoIf(IsSetWord32<Map::ConstructionCounterBits>(bit_field3),
         &slack_tracking);
  Comment("No slack tracking");
  InitializeJSObjectBodyNoSlackTracking(object, map, instance_size);
  Goto(&end);

  BIND(&slack_tracking);
  {
    Comment("Decrease construction counter");
    // Slack tracking is only done on initial maps.
    CSA_ASSERT(this, IsUndefined(LoadMapBackPointer(map)));
    STATIC_ASSERT(Map::ConstructionCounterBits::kLastUsedBit == 31);
    TNode<Word32T> new_bit_field3 = Int32Sub(
        bit_field3, Int32Constant(1 << Map::ConstructionCounterBits::kShift));
    StoreObjectFieldNoWriteBarrier(map, Map::kBitField3Offset, new_bit_field3,
                                   MachineRepresentation::kWord32);
    STATIC_ASSERT(Map::kSlackTrackingCounterEnd == 1);

    // The object still has in-object slack therefore the |unsed_or_unused|
    // field contain the "used" value.
    TNode<UintPtrT> used_size = TimesTaggedSize(ChangeUint32ToWord(
        LoadObjectField(map, Map::kUsedOrUnusedInstanceSizeInWordsOffset,
                        MachineType::Uint8())));

    Comment("iInitialize filler fields");
    InitializeFieldsWithRoot(object, used_size, instance_size,
                             RootIndex::kOnePointerFillerMap);

    Comment("Initialize undefined fields");
    InitializeFieldsWithRoot(object, IntPtrConstant(start_offset), used_size,
                             RootIndex::kUndefinedValue);

    STATIC_ASSERT(Map::kNoSlackTracking == 0);
    GotoIf(IsClearWord32<Map::ConstructionCounterBits>(new_bit_field3),
           &complete);
    Goto(&end);
  }

  // Finalize the instance size.
  BIND(&complete);
  {
    // ComplextInobjectSlackTracking doesn't allocate and thus doesn't need a
    // context.
    CallRuntime(Runtime::kCompleteInobjectSlackTrackingForMap,
                NoContextConstant(), map);
    Goto(&end);
  }

  BIND(&end);
}

void CodeStubAssembler::StoreFieldsNoWriteBarrier(Node* start_address,
                                                  Node* end_address,
                                                  Node* value) {
  Comment("StoreFieldsNoWriteBarrier");
  CSA_ASSERT(this, WordIsAligned(start_address, kTaggedSize));
  CSA_ASSERT(this, WordIsAligned(end_address, kTaggedSize));
  BuildFastLoop(
      start_address, end_address,
      [this, value](Node* current) {
        UnsafeStoreNoWriteBarrier(MachineRepresentation::kTagged, current,
                                  value);
      },
      kTaggedSize, INTPTR_PARAMETERS, IndexAdvanceMode::kPost);
}

TNode<BoolT> CodeStubAssembler::IsValidFastJSArrayCapacity(
    Node* capacity, ParameterMode capacity_mode) {
  return UncheckedCast<BoolT>(
      UintPtrLessThanOrEqual(ParameterToIntPtr(capacity, capacity_mode),
                             IntPtrConstant(JSArray::kMaxFastArrayLength)));
}

TNode<JSArray> CodeStubAssembler::AllocateJSArray(
    TNode<Map> array_map, TNode<FixedArrayBase> elements, TNode<Smi> length,
    Node* allocation_site, int array_header_size) {
  Comment("begin allocation of JSArray passing in elements");
  CSA_SLOW_ASSERT(this, TaggedIsPositiveSmi(length));

  int base_size = array_header_size;
  if (allocation_site != nullptr) {
    base_size += AllocationMemento::kSize;
  }

  TNode<IntPtrT> size = IntPtrConstant(base_size);
  TNode<JSArray> result =
      AllocateUninitializedJSArray(array_map, length, allocation_site, size);
  StoreObjectFieldNoWriteBarrier(result, JSArray::kElementsOffset, elements);
  return result;
}

std::pair<TNode<JSArray>, TNode<FixedArrayBase>>
CodeStubAssembler::AllocateUninitializedJSArrayWithElements(
    ElementsKind kind, TNode<Map> array_map, TNode<Smi> length,
    Node* allocation_site, Node* capacity, ParameterMode capacity_mode,
    AllocationFlags allocation_flags, int array_header_size) {
  Comment("begin allocation of JSArray with elements");
  CHECK_EQ(allocation_flags & ~kAllowLargeObjectAllocation, 0);
  CSA_SLOW_ASSERT(this, TaggedIsPositiveSmi(length));

  TVARIABLE(JSArray, array);
  TVARIABLE(FixedArrayBase, elements);

  Label out(this), empty(this), nonempty(this);

  int capacity_int;
  if (TryGetIntPtrOrSmiConstantValue(capacity, &capacity_int, capacity_mode)) {
    if (capacity_int == 0) {
      TNode<FixedArray> empty_array = EmptyFixedArrayConstant();
      array = AllocateJSArray(array_map, empty_array, length, allocation_site,
                              array_header_size);
      return {array.value(), empty_array};
    } else {
      Goto(&nonempty);
    }
  } else {
    Branch(SmiEqual(ParameterToTagged(capacity, capacity_mode), SmiConstant(0)),
           &empty, &nonempty);

    BIND(&empty);
    {
      TNode<FixedArray> empty_array = EmptyFixedArrayConstant();
      array = AllocateJSArray(array_map, empty_array, length, allocation_site,
                              array_header_size);
      elements = empty_array;
      Goto(&out);
    }
  }

  BIND(&nonempty);
  {
    int base_size = array_header_size;
    if (allocation_site != nullptr) base_size += AllocationMemento::kSize;

    const int elements_offset = base_size;

    // Compute space for elements
    base_size += FixedArray::kHeaderSize;
    TNode<IntPtrT> size =
        ElementOffsetFromIndex(capacity, kind, capacity_mode, base_size);

    // For very large arrays in which the requested allocation exceeds the
    // maximal size of a regular heap object, we cannot use the allocation
    // folding trick. Instead, we first allocate the elements in large object
    // space, and then allocate the JSArray (and possibly the allocation
    // memento) in new space.
    if (allocation_flags & kAllowLargeObjectAllocation) {
      Label next(this);
      GotoIf(IsRegularHeapObjectSize(size), &next);

      CSA_CHECK(this, IsValidFastJSArrayCapacity(capacity, capacity_mode));

      // Allocate and initialize the elements first. Full initialization is
      // needed because the upcoming JSArray allocation could trigger GC.
      elements =
          AllocateFixedArray(kind, capacity, capacity_mode, allocation_flags);

      if (IsDoubleElementsKind(kind)) {
        FillFixedDoubleArrayWithZero(
            CAST(elements.value()), ParameterToIntPtr(capacity, capacity_mode));
      } else {
        FillFixedArrayWithSmiZero(CAST(elements.value()),
                                  ParameterToIntPtr(capacity, capacity_mode));
      }

      // The JSArray and possibly allocation memento next. Note that
      // allocation_flags are *not* passed on here and the resulting JSArray
      // will always be in new space.
      array = AllocateJSArray(array_map, elements.value(), length,
                              allocation_site, array_header_size);

      Goto(&out);

      BIND(&next);
    }

    // Fold all objects into a single new space allocation.
    array =
        AllocateUninitializedJSArray(array_map, length, allocation_site, size);
    elements = UncheckedCast<FixedArrayBase>(
        InnerAllocate(array.value(), elements_offset));

    StoreObjectFieldNoWriteBarrier(array.value(), JSObject::kElementsOffset,
                                   elements.value());

    // Setup elements object.
    STATIC_ASSERT(FixedArrayBase::kHeaderSize == 2 * kTaggedSize);
    RootIndex elements_map_index = IsDoubleElementsKind(kind)
                                       ? RootIndex::kFixedDoubleArrayMap
                                       : RootIndex::kFixedArrayMap;
    DCHECK(RootsTable::IsImmortalImmovable(elements_map_index));
    StoreMapNoWriteBarrier(elements.value(), elements_map_index);

    TNode<Smi> capacity_smi = ParameterToTagged(capacity, capacity_mode);
    CSA_ASSERT(this, SmiGreaterThan(capacity_smi, SmiConstant(0)));
    StoreObjectFieldNoWriteBarrier(elements.value(), FixedArray::kLengthOffset,
                                   capacity_smi);
    Goto(&out);
  }

  BIND(&out);
  return {array.value(), elements.value()};
}

TNode<JSArray> CodeStubAssembler::AllocateUninitializedJSArray(
    TNode<Map> array_map, TNode<Smi> length, Node* allocation_site,
    TNode<IntPtrT> size_in_bytes) {
  CSA_SLOW_ASSERT(this, TaggedIsPositiveSmi(length));

  // Allocate space for the JSArray and the elements FixedArray in one go.
  TNode<HeapObject> array = AllocateInNewSpace(size_in_bytes);

  StoreMapNoWriteBarrier(array, array_map);
  StoreObjectFieldNoWriteBarrier(array, JSArray::kLengthOffset, length);
  StoreObjectFieldRoot(array, JSArray::kPropertiesOrHashOffset,
                       RootIndex::kEmptyFixedArray);

  if (allocation_site != nullptr) {
    InitializeAllocationMemento(array, IntPtrConstant(JSArray::kSize),
                                allocation_site);
  }

  return CAST(array);
}

TNode<JSArray> CodeStubAssembler::AllocateJSArray(
    ElementsKind kind, TNode<Map> array_map, Node* capacity, TNode<Smi> length,
    Node* allocation_site, ParameterMode capacity_mode,
    AllocationFlags allocation_flags) {
  CSA_SLOW_ASSERT(this, TaggedIsPositiveSmi(length));
  CSA_SLOW_ASSERT(this, MatchesParameterMode(capacity, capacity_mode));

  TNode<JSArray> array;
  TNode<FixedArrayBase> elements;

  std::tie(array, elements) = AllocateUninitializedJSArrayWithElements(
      kind, array_map, length, allocation_site, capacity, capacity_mode,
      allocation_flags);

  Label out(this), nonempty(this);

  Branch(SmiEqual(ParameterToTagged(capacity, capacity_mode), SmiConstant(0)),
         &out, &nonempty);

  BIND(&nonempty);
  {
    FillFixedArrayWithValue(kind, elements,
                            IntPtrOrSmiConstant(0, capacity_mode), capacity,
                            RootIndex::kTheHoleValue, capacity_mode);
    Goto(&out);
  }

  BIND(&out);
  return array;
}

Node* CodeStubAssembler::ExtractFastJSArray(Node* context, Node* array,
                                            Node* begin, Node* count,
                                            ParameterMode mode, Node* capacity,
                                            Node* allocation_site) {
  TNode<Map> original_array_map = LoadMap(array);
  TNode<Int32T> elements_kind = LoadMapElementsKind(original_array_map);

  // Use the cannonical map for the Array's ElementsKind
  TNode<NativeContext> native_context = LoadNativeContext(context);
  TNode<Map> array_map = LoadJSArrayElementsMap(elements_kind, native_context);

  TNode<FixedArrayBase> new_elements = ExtractFixedArray(
      LoadElements(array), begin, count, capacity,
      ExtractFixedArrayFlag::kAllFixedArrays, mode, nullptr, elements_kind);

  TNode<JSArray> result = AllocateJSArray(
      array_map, new_elements, ParameterToTagged(count, mode), allocation_site);
  return result;
}

Node* CodeStubAssembler::CloneFastJSArray(Node* context, Node* array,
                                          ParameterMode mode,
                                          Node* allocation_site,
                                          HoleConversionMode convert_holes) {
  // TODO(dhai): we should be able to assert IsFastJSArray(array) here, but this
  // function is also used to copy boilerplates even when the no-elements
  // protector is invalid. This function should be renamed to reflect its uses.
  CSA_ASSERT(this, IsJSArray(array));

  TNode<Number> length = LoadJSArrayLength(array);
  Node* new_elements = nullptr;
  VARIABLE(var_new_elements, MachineRepresentation::kTagged);
  TVARIABLE(Int32T, var_elements_kind, LoadMapElementsKind(LoadMap(array)));

  Label allocate_jsarray(this), holey_extract(this),
      allocate_jsarray_main(this);

  bool need_conversion =
      convert_holes == HoleConversionMode::kConvertToUndefined;
  if (need_conversion) {
    // We need to take care of holes, if the array is of holey elements kind.
    GotoIf(IsHoleyFastElementsKindForRead(var_elements_kind.value()),
           &holey_extract);
  }

  // Simple extraction that preserves holes.
  new_elements =
      ExtractFixedArray(LoadElements(array), IntPtrOrSmiConstant(0, mode),
                        TaggedToParameter(CAST(length), mode), nullptr,
                        ExtractFixedArrayFlag::kAllFixedArraysDontCopyCOW, mode,
                        nullptr, var_elements_kind.value());
  var_new_elements.Bind(new_elements);
  Goto(&allocate_jsarray);

  if (need_conversion) {
    BIND(&holey_extract);
    // Convert holes to undefined.
    TVARIABLE(BoolT, var_holes_converted, Int32FalseConstant());
    // Copy |array|'s elements store. The copy will be compatible with the
    // original elements kind unless there are holes in the source. Any holes
    // get converted to undefined, hence in that case the copy is compatible
    // only with PACKED_ELEMENTS and HOLEY_ELEMENTS, and we will choose
    // PACKED_ELEMENTS. Also, if we want to replace holes, we must not use
    // ExtractFixedArrayFlag::kDontCopyCOW.
    new_elements = ExtractFixedArray(
        LoadElements(array), IntPtrOrSmiConstant(0, mode),
        TaggedToParameter(CAST(length), mode), nullptr,
        ExtractFixedArrayFlag::kAllFixedArrays, mode, &var_holes_converted);
    var_new_elements.Bind(new_elements);
    // If the array type didn't change, use the original elements kind.
    GotoIfNot(var_holes_converted.value(), &allocate_jsarray);
    // Otherwise use PACKED_ELEMENTS for the target's elements kind.
    var_elements_kind = Int32Constant(PACKED_ELEMENTS);
    Goto(&allocate_jsarray);
  }

  BIND(&allocate_jsarray);

  // Handle any nonextensible elements kinds
  CSA_ASSERT(this, IsElementsKindLessThanOrEqual(
                       var_elements_kind.value(),
                       LAST_ANY_NONEXTENSIBLE_ELEMENTS_KIND));
  GotoIf(IsElementsKindLessThanOrEqual(var_elements_kind.value(),
                                       LAST_FAST_ELEMENTS_KIND),
         &allocate_jsarray_main);
  var_elements_kind = Int32Constant(PACKED_ELEMENTS);
  Goto(&allocate_jsarray_main);

  BIND(&allocate_jsarray_main);
  // Use the cannonical map for the chosen elements kind.
  TNode<NativeContext> native_context = LoadNativeContext(context);
  TNode<Map> array_map =
      LoadJSArrayElementsMap(var_elements_kind.value(), native_context);

  TNode<JSArray> result = AllocateJSArray(
      array_map, CAST(var_new_elements.value()), CAST(length), allocation_site);
  return result;
}

TNode<FixedArrayBase> CodeStubAssembler::AllocateFixedArray(
    ElementsKind kind, Node* capacity, ParameterMode mode,
    AllocationFlags flags, SloppyTNode<Map> fixed_array_map) {
  Comment("AllocateFixedArray");
  CSA_SLOW_ASSERT(this, MatchesParameterMode(capacity, mode));
  CSA_ASSERT(this, IntPtrOrSmiGreaterThan(capacity,
                                          IntPtrOrSmiConstant(0, mode), mode));

  const intptr_t kMaxLength = IsDoubleElementsKind(kind)
                                  ? FixedDoubleArray::kMaxLength
                                  : FixedArray::kMaxLength;
  intptr_t capacity_constant;
  if (ToParameterConstant(capacity, &capacity_constant, mode)) {
    CHECK_LE(capacity_constant, kMaxLength);
  } else {
    Label if_out_of_memory(this, Label::kDeferred), next(this);
    Branch(IntPtrOrSmiGreaterThan(
               capacity,
               IntPtrOrSmiConstant(static_cast<int>(kMaxLength), mode), mode),
           &if_out_of_memory, &next);

    BIND(&if_out_of_memory);
    CallRuntime(Runtime::kFatalProcessOutOfMemoryInvalidArrayLength,
                NoContextConstant());
    Unreachable();

    BIND(&next);
  }

  TNode<IntPtrT> total_size = GetFixedArrayAllocationSize(capacity, kind, mode);

  if (IsDoubleElementsKind(kind)) flags |= kDoubleAlignment;
  // Allocate both array and elements object, and initialize the JSArray.
  TNode<HeapObject> array = Allocate(total_size, flags);
  if (fixed_array_map != nullptr) {
    // Conservatively only skip the write barrier if there are no allocation
    // flags, this ensures that the object hasn't ended up in LOS. Note that the
    // fixed array map is currently always immortal and technically wouldn't
    // need the write barrier even in LOS, but it's better to not take chances
    // in case this invariant changes later, since it's difficult to enforce
    // locally here.
    if (flags == CodeStubAssembler::kNone) {
      StoreMapNoWriteBarrier(array, fixed_array_map);
    } else {
      StoreMap(array, fixed_array_map);
    }
  } else {
    RootIndex map_index = IsDoubleElementsKind(kind)
                              ? RootIndex::kFixedDoubleArrayMap
                              : RootIndex::kFixedArrayMap;
    DCHECK(RootsTable::IsImmortalImmovable(map_index));
    StoreMapNoWriteBarrier(array, map_index);
  }
  StoreObjectFieldNoWriteBarrier(array, FixedArrayBase::kLengthOffset,
                                 ParameterToTagged(capacity, mode));
  return UncheckedCast<FixedArrayBase>(array);
}

TNode<FixedArray> CodeStubAssembler::ExtractToFixedArray(
    SloppyTNode<FixedArrayBase> source, Node* first, Node* count,
    Node* capacity, SloppyTNode<Map> source_map, ElementsKind from_kind,
    AllocationFlags allocation_flags, ExtractFixedArrayFlags extract_flags,
    ParameterMode parameter_mode, HoleConversionMode convert_holes,
    TVariable<BoolT>* var_holes_converted, Node* source_elements_kind) {
  DCHECK_NE(first, nullptr);
  DCHECK_NE(count, nullptr);
  DCHECK_NE(capacity, nullptr);
  DCHECK(extract_flags & ExtractFixedArrayFlag::kFixedArrays);
  CSA_ASSERT(this, IntPtrOrSmiNotEqual(IntPtrOrSmiConstant(0, parameter_mode),
                                       capacity, parameter_mode));
  CSA_ASSERT(this, TaggedEqual(source_map, LoadMap(source)));

  TVARIABLE(FixedArrayBase, var_result);
  TVARIABLE(Map, var_target_map, source_map);

  Label done(this, {&var_result}), is_cow(this),
      new_space_check(this, {&var_target_map});

  // If source_map is either FixedDoubleArrayMap, or FixedCOWArrayMap but
  // we can't just use COW, use FixedArrayMap as the target map. Otherwise, use
  // source_map as the target map.
  if (IsDoubleElementsKind(from_kind)) {
    CSA_ASSERT(this, IsFixedDoubleArrayMap(source_map));
    var_target_map = FixedArrayMapConstant();
    Goto(&new_space_check);
  } else {
    CSA_ASSERT(this, Word32BinaryNot(IsFixedDoubleArrayMap(source_map)));
    Branch(TaggedEqual(var_target_map.value(), FixedCOWArrayMapConstant()),
           &is_cow, &new_space_check);

    BIND(&is_cow);
    {
      // |source| is a COW array, so we don't actually need to allocate a new
      // array unless:
      // 1) |extract_flags| forces us to, or
      // 2) we're asked to extract only part of the |source| (|first| != 0).
      if (extract_flags & ExtractFixedArrayFlag::kDontCopyCOW) {
        Branch(IntPtrOrSmiNotEqual(IntPtrOrSmiConstant(0, parameter_mode),
                                   first, parameter_mode),
               &new_space_check, [&] {
                 var_result = source;
                 Goto(&done);
               });
      } else {
        var_target_map = FixedArrayMapConstant();
        Goto(&new_space_check);
      }
    }
  }

  BIND(&new_space_check);
  {
    bool handle_old_space = !FLAG_young_generation_large_objects;
    if (handle_old_space) {
      if (extract_flags & ExtractFixedArrayFlag::kNewSpaceAllocationOnly) {
        handle_old_space = false;
        CSA_ASSERT(this, Word32BinaryNot(FixedArraySizeDoesntFitInNewSpace(
                             count, FixedArray::kHeaderSize, parameter_mode)));
      } else {
        int constant_count;
        handle_old_space =
            !TryGetIntPtrOrSmiConstantValue(count, &constant_count,
                                            parameter_mode) ||
            (constant_count >
             FixedArray::GetMaxLengthForNewSpaceAllocation(PACKED_ELEMENTS));
      }
    }

    Label old_space(this, Label::kDeferred);
    if (handle_old_space) {
      GotoIfFixedArraySizeDoesntFitInNewSpace(
          capacity, &old_space, FixedArray::kHeaderSize, parameter_mode);
    }

    Comment("Copy FixedArray in young generation");
    // We use PACKED_ELEMENTS to tell AllocateFixedArray and
    // CopyFixedArrayElements that we want a FixedArray.
    const ElementsKind to_kind = PACKED_ELEMENTS;
    TNode<FixedArrayBase> to_elements =
        AllocateFixedArray(to_kind, capacity, parameter_mode, allocation_flags,
                           var_target_map.value());
    var_result = to_elements;

#ifndef V8_ENABLE_SINGLE_GENERATION
#ifdef DEBUG
    TNode<IntPtrT> object_word = BitcastTaggedToWord(to_elements);
    TNode<IntPtrT> object_page = PageFromAddress(object_word);
    TNode<IntPtrT> page_flags =
        UncheckedCast<IntPtrT>(Load(MachineType::IntPtr(), object_page,
                                    IntPtrConstant(Page::kFlagsOffset)));
    CSA_ASSERT(
        this,
        WordNotEqual(
            WordAnd(page_flags,
                    IntPtrConstant(MemoryChunk::kIsInYoungGenerationMask)),
            IntPtrConstant(0)));
#endif
#endif

    if (convert_holes == HoleConversionMode::kDontConvert &&
        !IsDoubleElementsKind(from_kind)) {
      // We can use CopyElements (memcpy) because we don't need to replace or
      // convert any values. Since {to_elements} is in new-space, CopyElements
      // will efficiently use memcpy.
      FillFixedArrayWithValue(to_kind, to_elements, count, capacity,
                              RootIndex::kTheHoleValue, parameter_mode);
      CopyElements(to_kind, to_elements, IntPtrConstant(0), source,
                   ParameterToIntPtr(first, parameter_mode),
                   ParameterToIntPtr(count, parameter_mode),
                   SKIP_WRITE_BARRIER);
    } else {
      CopyFixedArrayElements(from_kind, source, to_kind, to_elements, first,
                             count, capacity, SKIP_WRITE_BARRIER,
                             parameter_mode, convert_holes,
                             var_holes_converted);
    }
    Goto(&done);

    if (handle_old_space) {
      BIND(&old_space);
      {
        Comment("Copy FixedArray in old generation");
        Label copy_one_by_one(this);

        // Try to use memcpy if we don't need to convert holes to undefined.
        if (convert_holes == HoleConversionMode::kDontConvert &&
            source_elements_kind != nullptr) {
          // Only try memcpy if we're not copying object pointers.
          GotoIfNot(IsFastSmiElementsKind(source_elements_kind),
                    &copy_one_by_one);

          const ElementsKind to_smi_kind = PACKED_SMI_ELEMENTS;
          to_elements =
              AllocateFixedArray(to_smi_kind, capacity, parameter_mode,
                                 allocation_flags, var_target_map.value());
          var_result = to_elements;

          FillFixedArrayWithValue(to_smi_kind, to_elements, count, capacity,
                                  RootIndex::kTheHoleValue, parameter_mode);
          // CopyElements will try to use memcpy if it's not conflicting with
          // GC. Otherwise it will copy elements by elements, but skip write
          // barriers (since we're copying smis to smis).
          CopyElements(to_smi_kind, to_elements, IntPtrConstant(0), source,
                       ParameterToIntPtr(first, parameter_mode),
                       ParameterToIntPtr(count, parameter_mode),
                       SKIP_WRITE_BARRIER);
          Goto(&done);
        } else {
          Goto(&copy_one_by_one);
        }

        BIND(&copy_one_by_one);
        {
          to_elements =
              AllocateFixedArray(to_kind, capacity, parameter_mode,
                                 allocation_flags, var_target_map.value());
          var_result = to_elements;
          CopyFixedArrayElements(from_kind, source, to_kind, to_elements, first,
                                 count, capacity, UPDATE_WRITE_BARRIER,
                                 parameter_mode, convert_holes,
                                 var_holes_converted);
          Goto(&done);
        }
      }
    }
  }

  BIND(&done);
  return UncheckedCast<FixedArray>(var_result.value());
}

TNode<FixedArrayBase> CodeStubAssembler::ExtractFixedDoubleArrayFillingHoles(
    Node* from_array, Node* first, Node* count, Node* capacity,
    Node* fixed_array_map, TVariable<BoolT>* var_holes_converted,
    AllocationFlags allocation_flags, ExtractFixedArrayFlags extract_flags,
    ParameterMode mode) {
  DCHECK_NE(first, nullptr);
  DCHECK_NE(count, nullptr);
  DCHECK_NE(capacity, nullptr);
  DCHECK_NE(var_holes_converted, nullptr);
  CSA_ASSERT(this, IsFixedDoubleArrayMap(fixed_array_map));

  VARIABLE(var_result, MachineRepresentation::kTagged);
  const ElementsKind kind = PACKED_DOUBLE_ELEMENTS;
  TNode<FixedArrayBase> to_elements = AllocateFixedArray(
      kind, capacity, mode, allocation_flags, fixed_array_map);
  var_result.Bind(to_elements);
  // We first try to copy the FixedDoubleArray to a new FixedDoubleArray.
  // |var_holes_converted| is set to False preliminarily.
  *var_holes_converted = Int32FalseConstant();

  // The construction of the loop and the offsets for double elements is
  // extracted from CopyFixedArrayElements.
  CSA_SLOW_ASSERT(this, MatchesParameterMode(count, mode));
  CSA_SLOW_ASSERT(this, MatchesParameterMode(capacity, mode));
  CSA_SLOW_ASSERT(this, IsFixedArrayWithKindOrEmpty(from_array, kind));
  STATIC_ASSERT(FixedArray::kHeaderSize == FixedDoubleArray::kHeaderSize);

  Comment("[ ExtractFixedDoubleArrayFillingHoles");

  // This copy can trigger GC, so we pre-initialize the array with holes.
  FillFixedArrayWithValue(kind, to_elements, IntPtrOrSmiConstant(0, mode),
                          capacity, RootIndex::kTheHoleValue, mode);

  const int first_element_offset = FixedArray::kHeaderSize - kHeapObjectTag;
  TNode<IntPtrT> first_from_element_offset =
      ElementOffsetFromIndex(first, kind, mode, 0);
  TNode<WordT> limit_offset = IntPtrAdd(first_from_element_offset,
                                        IntPtrConstant(first_element_offset));
  TVARIABLE(IntPtrT, var_from_offset,
            ElementOffsetFromIndex(IntPtrOrSmiAdd(first, count, mode), kind,
                                   mode, first_element_offset));

  Label decrement(this, {&var_from_offset}), done(this);
  TNode<WordT> to_array_adjusted =
      IntPtrSub(BitcastTaggedToWord(to_elements), first_from_element_offset);

  Branch(WordEqual(var_from_offset.value(), limit_offset), &done, &decrement);

  BIND(&decrement);
  {
    TNode<IntPtrT> from_offset =
        IntPtrSub(var_from_offset.value(), IntPtrConstant(kDoubleSize));
    var_from_offset = from_offset;

    Node* to_offset = from_offset;

    Label if_hole(this);

    Node* value = LoadElementAndPrepareForStore(
        from_array, var_from_offset.value(), kind, kind, &if_hole);

    StoreNoWriteBarrier(MachineRepresentation::kFloat64, to_array_adjusted,
                        to_offset, value);

    TNode<BoolT> compare = WordNotEqual(from_offset, limit_offset);
    Branch(compare, &decrement, &done);

    BIND(&if_hole);
    // We are unlucky: there are holes! We need to restart the copy, this time
    // we will copy the FixedDoubleArray to a new FixedArray with undefined
    // replacing holes. We signal this to the caller through
    // |var_holes_converted|.
    *var_holes_converted = Int32TrueConstant();
    to_elements =
        ExtractToFixedArray(from_array, first, count, capacity, fixed_array_map,
                            kind, allocation_flags, extract_flags, mode,
                            HoleConversionMode::kConvertToUndefined);
    var_result.Bind(to_elements);
    Goto(&done);
  }

  BIND(&done);
  Comment("] ExtractFixedDoubleArrayFillingHoles");
  return UncheckedCast<FixedArrayBase>(var_result.value());
}

TNode<FixedArrayBase> CodeStubAssembler::ExtractFixedArray(
    Node* source, Node* first, Node* count, Node* capacity,
    ExtractFixedArrayFlags extract_flags, ParameterMode parameter_mode,
    TVariable<BoolT>* var_holes_converted, Node* source_runtime_kind) {
  DCHECK(extract_flags & ExtractFixedArrayFlag::kFixedArrays ||
         extract_flags & ExtractFixedArrayFlag::kFixedDoubleArrays);
  // If we want to replace holes, ExtractFixedArrayFlag::kDontCopyCOW should not
  // be used, because that disables the iteration which detects holes.
  DCHECK_IMPLIES(var_holes_converted != nullptr,
                 !(extract_flags & ExtractFixedArrayFlag::kDontCopyCOW));
  HoleConversionMode convert_holes =
      var_holes_converted != nullptr ? HoleConversionMode::kConvertToUndefined
                                     : HoleConversionMode::kDontConvert;
  VARIABLE(var_result, MachineRepresentation::kTagged);
  const AllocationFlags allocation_flags =
      (extract_flags & ExtractFixedArrayFlag::kNewSpaceAllocationOnly)
          ? CodeStubAssembler::kNone
          : CodeStubAssembler::kAllowLargeObjectAllocation;
  if (first == nullptr) {
    first = IntPtrOrSmiConstant(0, parameter_mode);
  }
  if (count == nullptr) {
    count = IntPtrOrSmiSub(
        TaggedToParameter(LoadFixedArrayBaseLength(source), parameter_mode),
        first, parameter_mode);

    CSA_ASSERT(
        this, IntPtrOrSmiLessThanOrEqual(IntPtrOrSmiConstant(0, parameter_mode),
                                         count, parameter_mode));
  }
  if (capacity == nullptr) {
    capacity = count;
  } else {
    CSA_ASSERT(this, Word32BinaryNot(IntPtrOrSmiGreaterThan(
                         IntPtrOrSmiAdd(first, count, parameter_mode), capacity,
                         parameter_mode)));
  }

  Label if_fixed_double_array(this), empty(this), done(this, {&var_result});
  TNode<Map> source_map = LoadMap(source);
  GotoIf(IntPtrOrSmiEqual(IntPtrOrSmiConstant(0, parameter_mode), capacity,
                          parameter_mode),
         &empty);

  if (extract_flags & ExtractFixedArrayFlag::kFixedDoubleArrays) {
    if (extract_flags & ExtractFixedArrayFlag::kFixedArrays) {
      GotoIf(IsFixedDoubleArrayMap(source_map), &if_fixed_double_array);
    } else {
      CSA_ASSERT(this, IsFixedDoubleArrayMap(source_map));
    }
  }

  if (extract_flags & ExtractFixedArrayFlag::kFixedArrays) {
    // Here we can only get |source| as FixedArray, never FixedDoubleArray.
    // PACKED_ELEMENTS is used to signify that the source is a FixedArray.
    TNode<FixedArray> to_elements = ExtractToFixedArray(
        source, first, count, capacity, source_map, PACKED_ELEMENTS,
        allocation_flags, extract_flags, parameter_mode, convert_holes,
        var_holes_converted, source_runtime_kind);
    var_result.Bind(to_elements);
    Goto(&done);
  }

  if (extract_flags & ExtractFixedArrayFlag::kFixedDoubleArrays) {
    BIND(&if_fixed_double_array);
    Comment("Copy FixedDoubleArray");

    if (convert_holes == HoleConversionMode::kConvertToUndefined) {
      TNode<FixedArrayBase> to_elements = ExtractFixedDoubleArrayFillingHoles(
          source, first, count, capacity, source_map, var_holes_converted,
          allocation_flags, extract_flags, parameter_mode);
      var_result.Bind(to_elements);
    } else {
      // We use PACKED_DOUBLE_ELEMENTS to signify that both the source and
      // the target are FixedDoubleArray. That it is PACKED or HOLEY does not
      // matter.
      ElementsKind kind = PACKED_DOUBLE_ELEMENTS;
      TNode<FixedArrayBase> to_elements = AllocateFixedArray(
          kind, capacity, parameter_mode, allocation_flags, source_map);
      FillFixedArrayWithValue(kind, to_elements, count, capacity,
                              RootIndex::kTheHoleValue, parameter_mode);
      CopyElements(kind, to_elements, IntPtrConstant(0), CAST(source),
                   ParameterToIntPtr(first, parameter_mode),
                   ParameterToIntPtr(count, parameter_mode));
      var_result.Bind(to_elements);
    }

    Goto(&done);
  }

  BIND(&empty);
  {
    Comment("Copy empty array");

    var_result.Bind(EmptyFixedArrayConstant());
    Goto(&done);
  }

  BIND(&done);
  return UncheckedCast<FixedArray>(var_result.value());
}

void CodeStubAssembler::InitializePropertyArrayLength(Node* property_array,
                                                      Node* length,
                                                      ParameterMode mode) {
  CSA_SLOW_ASSERT(this, IsPropertyArray(property_array));
  CSA_ASSERT(
      this, IntPtrOrSmiGreaterThan(length, IntPtrOrSmiConstant(0, mode), mode));
  CSA_ASSERT(
      this,
      IntPtrOrSmiLessThanOrEqual(
          length, IntPtrOrSmiConstant(PropertyArray::LengthField::kMax, mode),
          mode));
  StoreObjectFieldNoWriteBarrier(
      property_array, PropertyArray::kLengthAndHashOffset,
      ParameterToTagged(length, mode), MachineRepresentation::kTaggedSigned);
}

Node* CodeStubAssembler::AllocatePropertyArray(Node* capacity_node,
                                               ParameterMode mode,
                                               AllocationFlags flags) {
  CSA_SLOW_ASSERT(this, MatchesParameterMode(capacity_node, mode));
  CSA_ASSERT(this, IntPtrOrSmiGreaterThan(capacity_node,
                                          IntPtrOrSmiConstant(0, mode), mode));
  TNode<IntPtrT> total_size =
      GetPropertyArrayAllocationSize(capacity_node, mode);

  TNode<HeapObject> array = Allocate(total_size, flags);
  RootIndex map_index = RootIndex::kPropertyArrayMap;
  DCHECK(RootsTable::IsImmortalImmovable(map_index));
  StoreMapNoWriteBarrier(array, map_index);
  InitializePropertyArrayLength(array, capacity_node, mode);
  return array;
}

void CodeStubAssembler::FillPropertyArrayWithUndefined(Node* array,
                                                       Node* from_node,
                                                       Node* to_node,
                                                       ParameterMode mode) {
  CSA_SLOW_ASSERT(this, MatchesParameterMode(from_node, mode));
  CSA_SLOW_ASSERT(this, MatchesParameterMode(to_node, mode));
  CSA_SLOW_ASSERT(this, IsPropertyArray(array));
  ElementsKind kind = PACKED_ELEMENTS;
  TNode<Oddball> value = UndefinedConstant();
  BuildFastFixedArrayForEach(
      array, kind, from_node, to_node,
      [this, value](Node* array, Node* offset) {
        StoreNoWriteBarrier(MachineRepresentation::kTagged, array, offset,
                            value);
      },
      mode);
}

void CodeStubAssembler::FillFixedArrayWithValue(ElementsKind kind, Node* array,
                                                Node* from_node, Node* to_node,
                                                RootIndex value_root_index,
                                                ParameterMode mode) {
  CSA_SLOW_ASSERT(this, MatchesParameterMode(from_node, mode));
  CSA_SLOW_ASSERT(this, MatchesParameterMode(to_node, mode));
  CSA_SLOW_ASSERT(this, IsFixedArrayWithKind(array, kind));
  DCHECK(value_root_index == RootIndex::kTheHoleValue ||
         value_root_index == RootIndex::kUndefinedValue);

  // Determine the value to initialize the {array} based
  // on the {value_root_index} and the elements {kind}.
  TNode<Object> value = LoadRoot(value_root_index);
  TNode<Float64T> float_value;
  if (IsDoubleElementsKind(kind)) {
    float_value = LoadHeapNumberValue(CAST(value));
  }

  BuildFastFixedArrayForEach(
      array, kind, from_node, to_node,
      [this, value, float_value, kind](Node* array, Node* offset) {
        if (IsDoubleElementsKind(kind)) {
          StoreNoWriteBarrier(MachineRepresentation::kFloat64, array, offset,
                              float_value);
        } else {
          StoreNoWriteBarrier(MachineRepresentation::kTagged, array, offset,
                              value);
        }
      },
      mode);
}

void CodeStubAssembler::StoreFixedDoubleArrayHole(
    TNode<FixedDoubleArray> array, Node* index, ParameterMode parameter_mode) {
  CSA_SLOW_ASSERT(this, MatchesParameterMode(index, parameter_mode));
  TNode<IntPtrT> offset =
      ElementOffsetFromIndex(index, PACKED_DOUBLE_ELEMENTS, parameter_mode,
                             FixedArray::kHeaderSize - kHeapObjectTag);
  CSA_ASSERT(this, IsOffsetInBounds(
                       offset, LoadAndUntagFixedArrayBaseLength(array),
                       FixedDoubleArray::kHeaderSize, PACKED_DOUBLE_ELEMENTS));
  TNode<UintPtrT> double_hole =
      Is64() ? ReinterpretCast<UintPtrT>(Int64Constant(kHoleNanInt64))
             : ReinterpretCast<UintPtrT>(Int32Constant(kHoleNanLower32));
  // TODO(danno): When we have a Float32/Float64 wrapper class that
  // preserves double bits during manipulation, remove this code/change
  // this to an indexed Float64 store.
  if (Is64()) {
    StoreNoWriteBarrier(MachineRepresentation::kWord64, array, offset,
                        double_hole);
  } else {
    StoreNoWriteBarrier(MachineRepresentation::kWord32, array, offset,
                        double_hole);
    StoreNoWriteBarrier(MachineRepresentation::kWord32, array,
                        IntPtrAdd(offset, IntPtrConstant(kInt32Size)),
                        double_hole);
  }
}

void CodeStubAssembler::FillFixedArrayWithSmiZero(TNode<FixedArray> array,
                                                  TNode<IntPtrT> length) {
  CSA_ASSERT(this, WordEqual(length, LoadAndUntagFixedArrayBaseLength(array)));

  TNode<IntPtrT> byte_length = TimesTaggedSize(length);
  CSA_ASSERT(this, UintPtrLessThan(length, byte_length));

  static const int32_t fa_base_data_offset =
      FixedArray::kHeaderSize - kHeapObjectTag;
  TNode<IntPtrT> backing_store = IntPtrAdd(BitcastTaggedToWord(array),
                                           IntPtrConstant(fa_base_data_offset));

  // Call out to memset to perform initialization.
  TNode<ExternalReference> memset =
      ExternalConstant(ExternalReference::libc_memset_function());
  STATIC_ASSERT(kSizetSize == kIntptrSize);
  CallCFunction(memset, MachineType::Pointer(),
                std::make_pair(MachineType::Pointer(), backing_store),
                std::make_pair(MachineType::IntPtr(), IntPtrConstant(0)),
                std::make_pair(MachineType::UintPtr(), byte_length));
}

void CodeStubAssembler::FillFixedDoubleArrayWithZero(
    TNode<FixedDoubleArray> array, TNode<IntPtrT> length) {
  CSA_ASSERT(this, WordEqual(length, LoadAndUntagFixedArrayBaseLength(array)));

  TNode<IntPtrT> byte_length = TimesDoubleSize(length);
  CSA_ASSERT(this, UintPtrLessThan(length, byte_length));

  static const int32_t fa_base_data_offset =
      FixedDoubleArray::kHeaderSize - kHeapObjectTag;
  TNode<IntPtrT> backing_store = IntPtrAdd(BitcastTaggedToWord(array),
                                           IntPtrConstant(fa_base_data_offset));

  // Call out to memset to perform initialization.
  TNode<ExternalReference> memset =
      ExternalConstant(ExternalReference::libc_memset_function());
  STATIC_ASSERT(kSizetSize == kIntptrSize);
  CallCFunction(memset, MachineType::Pointer(),
                std::make_pair(MachineType::Pointer(), backing_store),
                std::make_pair(MachineType::IntPtr(), IntPtrConstant(0)),
                std::make_pair(MachineType::UintPtr(), byte_length));
}

void CodeStubAssembler::JumpIfPointersFromHereAreInteresting(
    TNode<Object> object, Label* interesting) {
  Label finished(this);
  TNode<IntPtrT> object_word = BitcastTaggedToWord(object);
  TNode<IntPtrT> object_page = PageFromAddress(object_word);
  TNode<IntPtrT> page_flags = UncheckedCast<IntPtrT>(Load(
      MachineType::IntPtr(), object_page, IntPtrConstant(Page::kFlagsOffset)));
  Branch(
      WordEqual(WordAnd(page_flags,
                        IntPtrConstant(
                            MemoryChunk::kPointersFromHereAreInterestingMask)),
                IntPtrConstant(0)),
      &finished, interesting);
  BIND(&finished);
}

void CodeStubAssembler::MoveElements(ElementsKind kind,
                                     TNode<FixedArrayBase> elements,
                                     TNode<IntPtrT> dst_index,
                                     TNode<IntPtrT> src_index,
                                     TNode<IntPtrT> length) {
  Label finished(this);
  Label needs_barrier(this);
  const bool needs_barrier_check = !IsDoubleElementsKind(kind);

  DCHECK(IsFastElementsKind(kind));
  CSA_ASSERT(this, IsFixedArrayWithKind(elements, kind));
  CSA_ASSERT(this,
             IntPtrLessThanOrEqual(IntPtrAdd(dst_index, length),
                                   LoadAndUntagFixedArrayBaseLength(elements)));
  CSA_ASSERT(this,
             IntPtrLessThanOrEqual(IntPtrAdd(src_index, length),
                                   LoadAndUntagFixedArrayBaseLength(elements)));

  // The write barrier can be ignored if {dst_elements} is in new space, or if
  // the elements pointer is FixedDoubleArray.
  if (needs_barrier_check) {
    JumpIfPointersFromHereAreInteresting(elements, &needs_barrier);
  }

  const TNode<IntPtrT> source_byte_length =
      IntPtrMul(length, IntPtrConstant(ElementsKindToByteSize(kind)));
  static const int32_t fa_base_data_offset =
      FixedArrayBase::kHeaderSize - kHeapObjectTag;
  TNode<IntPtrT> elements_intptr = BitcastTaggedToWord(elements);
  TNode<IntPtrT> target_data_ptr =
      IntPtrAdd(elements_intptr,
                ElementOffsetFromIndex(dst_index, kind, INTPTR_PARAMETERS,
                                       fa_base_data_offset));
  TNode<IntPtrT> source_data_ptr =
      IntPtrAdd(elements_intptr,
                ElementOffsetFromIndex(src_index, kind, INTPTR_PARAMETERS,
                                       fa_base_data_offset));
  TNode<ExternalReference> memmove =
      ExternalConstant(ExternalReference::libc_memmove_function());
  CallCFunction(memmove, MachineType::Pointer(),
                std::make_pair(MachineType::Pointer(), target_data_ptr),
                std::make_pair(MachineType::Pointer(), source_data_ptr),
                std::make_pair(MachineType::UintPtr(), source_byte_length));

  if (needs_barrier_check) {
    Goto(&finished);

    BIND(&needs_barrier);
    {
      const TNode<IntPtrT> begin = src_index;
      const TNode<IntPtrT> end = IntPtrAdd(begin, length);

      // If dst_index is less than src_index, then walk forward.
      const TNode<IntPtrT> delta =
          IntPtrMul(IntPtrSub(dst_index, begin),
                    IntPtrConstant(ElementsKindToByteSize(kind)));
      auto loop_body = [&](Node* array, Node* offset) {
        Node* const element = Load(MachineType::AnyTagged(), array, offset);
        TNode<WordT> const delta_offset = IntPtrAdd(offset, delta);
        Store(array, delta_offset, element);
      };

      Label iterate_forward(this);
      Label iterate_backward(this);
      Branch(IntPtrLessThan(delta, IntPtrConstant(0)), &iterate_forward,
             &iterate_backward);
      BIND(&iterate_forward);
      {
        // Make a loop for the stores.
        BuildFastFixedArrayForEach(elements, kind, begin, end, loop_body,
                                   INTPTR_PARAMETERS,
                                   ForEachDirection::kForward);
        Goto(&finished);
      }

      BIND(&iterate_backward);
      {
        BuildFastFixedArrayForEach(elements, kind, begin, end, loop_body,
                                   INTPTR_PARAMETERS,
                                   ForEachDirection::kReverse);
        Goto(&finished);
      }
    }
    BIND(&finished);
  }
}

void CodeStubAssembler::CopyElements(ElementsKind kind,
                                     TNode<FixedArrayBase> dst_elements,
                                     TNode<IntPtrT> dst_index,
                                     TNode<FixedArrayBase> src_elements,
                                     TNode<IntPtrT> src_index,
                                     TNode<IntPtrT> length,
                                     WriteBarrierMode write_barrier) {
  Label finished(this);
  Label needs_barrier(this);
  const bool needs_barrier_check = !IsDoubleElementsKind(kind);

  DCHECK(IsFastElementsKind(kind));
  CSA_ASSERT(this, IsFixedArrayWithKind(dst_elements, kind));
  CSA_ASSERT(this, IsFixedArrayWithKind(src_elements, kind));
  CSA_ASSERT(this, IntPtrLessThanOrEqual(
                       IntPtrAdd(dst_index, length),
                       LoadAndUntagFixedArrayBaseLength(dst_elements)));
  CSA_ASSERT(this, IntPtrLessThanOrEqual(
                       IntPtrAdd(src_index, length),
                       LoadAndUntagFixedArrayBaseLength(src_elements)));
  CSA_ASSERT(this, Word32Or(TaggedNotEqual(dst_elements, src_elements),
                            IntPtrEqual(length, IntPtrConstant(0))));

  // The write barrier can be ignored if {dst_elements} is in new space, or if
  // the elements pointer is FixedDoubleArray.
  if (needs_barrier_check) {
    JumpIfPointersFromHereAreInteresting(dst_elements, &needs_barrier);
  }

  TNode<IntPtrT> source_byte_length =
      IntPtrMul(length, IntPtrConstant(ElementsKindToByteSize(kind)));
  static const int32_t fa_base_data_offset =
      FixedArrayBase::kHeaderSize - kHeapObjectTag;
  TNode<IntPtrT> src_offset_start = ElementOffsetFromIndex(
      src_index, kind, INTPTR_PARAMETERS, fa_base_data_offset);
  TNode<IntPtrT> dst_offset_start = ElementOffsetFromIndex(
      dst_index, kind, INTPTR_PARAMETERS, fa_base_data_offset);
  TNode<IntPtrT> src_elements_intptr = BitcastTaggedToWord(src_elements);
  TNode<IntPtrT> source_data_ptr =
      IntPtrAdd(src_elements_intptr, src_offset_start);
  TNode<IntPtrT> dst_elements_intptr = BitcastTaggedToWord(dst_elements);
  TNode<IntPtrT> dst_data_ptr =
      IntPtrAdd(dst_elements_intptr, dst_offset_start);
  TNode<ExternalReference> memcpy =
      ExternalConstant(ExternalReference::libc_memcpy_function());
  CallCFunction(memcpy, MachineType::Pointer(),
                std::make_pair(MachineType::Pointer(), dst_data_ptr),
                std::make_pair(MachineType::Pointer(), source_data_ptr),
                std::make_pair(MachineType::UintPtr(), source_byte_length));

  if (needs_barrier_check) {
    Goto(&finished);

    BIND(&needs_barrier);
    {
      const TNode<IntPtrT> begin = src_index;
      const TNode<IntPtrT> end = IntPtrAdd(begin, length);
      const TNode<IntPtrT> delta =
          IntPtrMul(IntPtrSub(dst_index, src_index),
                    IntPtrConstant(ElementsKindToByteSize(kind)));
      BuildFastFixedArrayForEach(
          src_elements, kind, begin, end,
          [&](Node* array, Node* offset) {
            Node* const element = Load(MachineType::AnyTagged(), array, offset);
            TNode<WordT> const delta_offset = IntPtrAdd(offset, delta);
            if (write_barrier == SKIP_WRITE_BARRIER) {
              StoreNoWriteBarrier(MachineRepresentation::kTagged, dst_elements,
                                  delta_offset, element);
            } else {
              Store(dst_elements, delta_offset, element);
            }
          },
          INTPTR_PARAMETERS, ForEachDirection::kForward);
      Goto(&finished);
    }
    BIND(&finished);
  }
}

void CodeStubAssembler::CopyFixedArrayElements(
    ElementsKind from_kind, Node* from_array, ElementsKind to_kind,
    Node* to_array, Node* first_element, Node* element_count, Node* capacity,
    WriteBarrierMode barrier_mode, ParameterMode mode,
    HoleConversionMode convert_holes, TVariable<BoolT>* var_holes_converted) {
  DCHECK_IMPLIES(var_holes_converted != nullptr,
                 convert_holes == HoleConversionMode::kConvertToUndefined);
  CSA_SLOW_ASSERT(this, MatchesParameterMode(element_count, mode));
  CSA_SLOW_ASSERT(this, MatchesParameterMode(capacity, mode));
  CSA_SLOW_ASSERT(this, IsFixedArrayWithKindOrEmpty(from_array, from_kind));
  CSA_SLOW_ASSERT(this, IsFixedArrayWithKindOrEmpty(to_array, to_kind));
  STATIC_ASSERT(FixedArray::kHeaderSize == FixedDoubleArray::kHeaderSize);
  const int first_element_offset = FixedArray::kHeaderSize - kHeapObjectTag;
  Comment("[ CopyFixedArrayElements");

  // Typed array elements are not supported.
  DCHECK(!IsTypedArrayElementsKind(from_kind));
  DCHECK(!IsTypedArrayElementsKind(to_kind));

  Label done(this);
  bool from_double_elements = IsDoubleElementsKind(from_kind);
  bool to_double_elements = IsDoubleElementsKind(to_kind);
  bool doubles_to_objects_conversion =
      IsDoubleElementsKind(from_kind) && IsObjectElementsKind(to_kind);
  bool needs_write_barrier =
      doubles_to_objects_conversion ||
      (barrier_mode == UPDATE_WRITE_BARRIER && IsObjectElementsKind(to_kind));
  bool element_offset_matches =
      !needs_write_barrier &&
      (kTaggedSize == kDoubleSize ||
       IsDoubleElementsKind(from_kind) == IsDoubleElementsKind(to_kind));
  TNode<UintPtrT> double_hole =
      Is64() ? ReinterpretCast<UintPtrT>(Int64Constant(kHoleNanInt64))
             : ReinterpretCast<UintPtrT>(Int32Constant(kHoleNanLower32));

  // If copying might trigger a GC, we pre-initialize the FixedArray such that
  // it's always in a consistent state.
  if (convert_holes == HoleConversionMode::kConvertToUndefined) {
    DCHECK(IsObjectElementsKind(to_kind));
    // Use undefined for the part that we copy and holes for the rest.
    // Later if we run into a hole in the source we can just skip the writing
    // to the target and are still guaranteed that we get an undefined.
    FillFixedArrayWithValue(to_kind, to_array, IntPtrOrSmiConstant(0, mode),
                            element_count, RootIndex::kUndefinedValue, mode);
    FillFixedArrayWithValue(to_kind, to_array, element_count, capacity,
                            RootIndex::kTheHoleValue, mode);
  } else if (doubles_to_objects_conversion) {
    // Pre-initialized the target with holes so later if we run into a hole in
    // the source we can just skip the writing to the target.
    FillFixedArrayWithValue(to_kind, to_array, IntPtrOrSmiConstant(0, mode),
                            capacity, RootIndex::kTheHoleValue, mode);
  } else if (element_count != capacity) {
    FillFixedArrayWithValue(to_kind, to_array, element_count, capacity,
                            RootIndex::kTheHoleValue, mode);
  }

  TNode<IntPtrT> first_from_element_offset =
      ElementOffsetFromIndex(first_element, from_kind, mode, 0);
  TNode<IntPtrT> limit_offset = Signed(IntPtrAdd(
      first_from_element_offset, IntPtrConstant(first_element_offset)));
  TVARIABLE(
      IntPtrT, var_from_offset,
      ElementOffsetFromIndex(IntPtrOrSmiAdd(first_element, element_count, mode),
                             from_kind, mode, first_element_offset));
  // This second variable is used only when the element sizes of source and
  // destination arrays do not match.
  VARIABLE(var_to_offset, MachineType::PointerRepresentation());
  if (element_offset_matches) {
    var_to_offset.Bind(var_from_offset.value());
  } else {
    var_to_offset.Bind(ElementOffsetFromIndex(element_count, to_kind, mode,
                                              first_element_offset));
  }

  Variable* vars[] = {&var_from_offset, &var_to_offset, var_holes_converted};
  int num_vars =
      var_holes_converted != nullptr ? arraysize(vars) : arraysize(vars) - 1;
  Label decrement(this, num_vars, vars);

  Node* to_array_adjusted =
      element_offset_matches
          ? IntPtrSub(BitcastTaggedToWord(to_array), first_from_element_offset)
          : to_array;

  Branch(WordEqual(var_from_offset.value(), limit_offset), &done, &decrement);

  BIND(&decrement);
  {
    TNode<IntPtrT> from_offset = Signed(IntPtrSub(
        var_from_offset.value(),
        IntPtrConstant(from_double_elements ? kDoubleSize : kTaggedSize)));
    var_from_offset = from_offset;

    Node* to_offset;
    if (element_offset_matches) {
      to_offset = from_offset;
    } else {
      to_offset = IntPtrSub(
          var_to_offset.value(),
          IntPtrConstant(to_double_elements ? kDoubleSize : kTaggedSize));
      var_to_offset.Bind(to_offset);
    }

    Label next_iter(this), store_double_hole(this), signal_hole(this);
    Label* if_hole;
    if (convert_holes == HoleConversionMode::kConvertToUndefined) {
      // The target elements array is already preinitialized with undefined
      // so we only need to signal that a hole was found and continue the loop.
      if_hole = &signal_hole;
    } else if (doubles_to_objects_conversion) {
      // The target elements array is already preinitialized with holes, so we
      // can just proceed with the next iteration.
      if_hole = &next_iter;
    } else if (IsDoubleElementsKind(to_kind)) {
      if_hole = &store_double_hole;
    } else {
      // In all the other cases don't check for holes and copy the data as is.
      if_hole = nullptr;
    }

    Node* value = LoadElementAndPrepareForStore(
        from_array, var_from_offset.value(), from_kind, to_kind, if_hole);

    if (needs_write_barrier) {
      CHECK_EQ(to_array, to_array_adjusted);
      Store(to_array_adjusted, to_offset, value);
    } else if (to_double_elements) {
      StoreNoWriteBarrier(MachineRepresentation::kFloat64, to_array_adjusted,
                          to_offset, value);
    } else {
      UnsafeStoreNoWriteBarrier(MachineRepresentation::kTagged,
                                to_array_adjusted, to_offset, value);
    }
    Goto(&next_iter);

    if (if_hole == &store_double_hole) {
      BIND(&store_double_hole);
      // Don't use doubles to store the hole double, since manipulating the
      // signaling NaN used for the hole in C++, e.g. with bit_cast, will
      // change its value on ia32 (the x87 stack is used to return values
      // and stores to the stack silently clear the signalling bit).
      //
      // TODO(danno): When we have a Float32/Float64 wrapper class that
      // preserves double bits during manipulation, remove this code/change
      // this to an indexed Float64 store.
      if (Is64()) {
        StoreNoWriteBarrier(MachineRepresentation::kWord64, to_array_adjusted,
                            to_offset, double_hole);
      } else {
        StoreNoWriteBarrier(MachineRepresentation::kWord32, to_array_adjusted,
                            to_offset, double_hole);
        StoreNoWriteBarrier(MachineRepresentation::kWord32, to_array_adjusted,
                            IntPtrAdd(to_offset, IntPtrConstant(kInt32Size)),
                            double_hole);
      }
      Goto(&next_iter);
    } else if (if_hole == &signal_hole) {
      // This case happens only when IsObjectElementsKind(to_kind).
      BIND(&signal_hole);
      if (var_holes_converted != nullptr) {
        *var_holes_converted = Int32TrueConstant();
      }
      Goto(&next_iter);
    }

    BIND(&next_iter);
    TNode<BoolT> compare = WordNotEqual(from_offset, limit_offset);
    Branch(compare, &decrement, &done);
  }

  BIND(&done);
  Comment("] CopyFixedArrayElements");
}

TNode<FixedArray> CodeStubAssembler::HeapObjectToFixedArray(
    TNode<HeapObject> base, Label* cast_fail) {
  Label fixed_array(this);
  TNode<Map> map = LoadMap(base);
  GotoIf(TaggedEqual(map, FixedArrayMapConstant()), &fixed_array);
  GotoIf(TaggedNotEqual(map, FixedCOWArrayMapConstant()), cast_fail);
  Goto(&fixed_array);
  BIND(&fixed_array);
  return UncheckedCast<FixedArray>(base);
}

void CodeStubAssembler::CopyPropertyArrayValues(Node* from_array,
                                                Node* to_array,
                                                Node* property_count,
                                                WriteBarrierMode barrier_mode,
                                                ParameterMode mode,
                                                DestroySource destroy_source) {
  CSA_SLOW_ASSERT(this, MatchesParameterMode(property_count, mode));
  CSA_SLOW_ASSERT(this, Word32Or(IsPropertyArray(from_array),
                                 IsEmptyFixedArray(from_array)));
  CSA_SLOW_ASSERT(this, IsPropertyArray(to_array));
  Comment("[ CopyPropertyArrayValues");

  bool needs_write_barrier = barrier_mode == UPDATE_WRITE_BARRIER;

  if (destroy_source == DestroySource::kNo) {
    // PropertyArray may contain mutable HeapNumbers, which will be cloned on
    // the heap, requiring a write barrier.
    needs_write_barrier = true;
  }

  Node* start = IntPtrOrSmiConstant(0, mode);
  ElementsKind kind = PACKED_ELEMENTS;
  BuildFastFixedArrayForEach(
      from_array, kind, start, property_count,
      [this, to_array, needs_write_barrier, destroy_source](Node* array,
                                                            Node* offset) {
        Node* value = Load(MachineType::AnyTagged(), array, offset);

        if (destroy_source == DestroySource::kNo) {
          value = CloneIfMutablePrimitive(CAST(value));
        }

        if (needs_write_barrier) {
          Store(to_array, offset, value);
        } else {
          StoreNoWriteBarrier(MachineRepresentation::kTagged, to_array, offset,
                              value);
        }
      },
      mode);

#ifdef DEBUG
  // Zap {from_array} if the copying above has made it invalid.
  if (destroy_source == DestroySource::kYes) {
    Label did_zap(this);
    GotoIf(IsEmptyFixedArray(from_array), &did_zap);
    FillPropertyArrayWithUndefined(from_array, start, property_count, mode);

    Goto(&did_zap);
    BIND(&did_zap);
  }
#endif
  Comment("] CopyPropertyArrayValues");
}

void CodeStubAssembler::CopyStringCharacters(Node* from_string, Node* to_string,
                                             TNode<IntPtrT> from_index,
                                             TNode<IntPtrT> to_index,
                                             TNode<IntPtrT> character_count,
                                             String::Encoding from_encoding,
                                             String::Encoding to_encoding) {
  // Cannot assert IsString(from_string) and IsString(to_string) here because
  // CSA::SubString can pass in faked sequential strings when handling external
  // subject strings.
  bool from_one_byte = from_encoding == String::ONE_BYTE_ENCODING;
  bool to_one_byte = to_encoding == String::ONE_BYTE_ENCODING;
  DCHECK_IMPLIES(to_one_byte, from_one_byte);
  Comment("CopyStringCharacters ",
          from_one_byte ? "ONE_BYTE_ENCODING" : "TWO_BYTE_ENCODING", " -> ",
          to_one_byte ? "ONE_BYTE_ENCODING" : "TWO_BYTE_ENCODING");

  ElementsKind from_kind = from_one_byte ? UINT8_ELEMENTS : UINT16_ELEMENTS;
  ElementsKind to_kind = to_one_byte ? UINT8_ELEMENTS : UINT16_ELEMENTS;
  STATIC_ASSERT(SeqOneByteString::kHeaderSize == SeqTwoByteString::kHeaderSize);
  int header_size = SeqOneByteString::kHeaderSize - kHeapObjectTag;
  TNode<IntPtrT> from_offset = ElementOffsetFromIndex(
      from_index, from_kind, INTPTR_PARAMETERS, header_size);
  TNode<IntPtrT> to_offset =
      ElementOffsetFromIndex(to_index, to_kind, INTPTR_PARAMETERS, header_size);
  TNode<IntPtrT> byte_count =
      ElementOffsetFromIndex(character_count, from_kind, INTPTR_PARAMETERS);
  TNode<WordT> limit_offset = IntPtrAdd(from_offset, byte_count);

  // Prepare the fast loop
  MachineType type =
      from_one_byte ? MachineType::Uint8() : MachineType::Uint16();
  MachineRepresentation rep = to_one_byte ? MachineRepresentation::kWord8
                                          : MachineRepresentation::kWord16;
  int from_increment = 1 << ElementsKindToShiftSize(from_kind);
  int to_increment = 1 << ElementsKindToShiftSize(to_kind);

  VARIABLE(current_to_offset, MachineType::PointerRepresentation(), to_offset);
  VariableList vars({&current_to_offset}, zone());
  int to_index_constant = 0, from_index_constant = 0;
  bool index_same = (from_encoding == to_encoding) &&
                    (from_index == to_index ||
                     (ToInt32Constant(from_index, &from_index_constant) &&
                      ToInt32Constant(to_index, &to_index_constant) &&
                      from_index_constant == to_index_constant));
  BuildFastLoop(
      vars, from_offset, limit_offset,
      [this, from_string, to_string, &current_to_offset, to_increment, type,
       rep, index_same](Node* offset) {
        Node* value = Load(type, from_string, offset);
        StoreNoWriteBarrier(rep, to_string,
                            index_same ? offset : current_to_offset.value(),
                            value);
        if (!index_same) {
          Increment(&current_to_offset, to_increment);
        }
      },
      from_increment, INTPTR_PARAMETERS, IndexAdvanceMode::kPost);
}

Node* CodeStubAssembler::LoadElementAndPrepareForStore(Node* array,
                                                       Node* offset,
                                                       ElementsKind from_kind,
                                                       ElementsKind to_kind,
                                                       Label* if_hole) {
  CSA_ASSERT(this, IsFixedArrayWithKind(array, from_kind));
  if (IsDoubleElementsKind(from_kind)) {
    TNode<Float64T> value =
        LoadDoubleWithHoleCheck(array, offset, if_hole, MachineType::Float64());
    if (!IsDoubleElementsKind(to_kind)) {
      return AllocateHeapNumberWithValue(value);
    }
    return value;

  } else {
    TNode<Object> value = CAST(Load(MachineType::AnyTagged(), array, offset));
    if (if_hole) {
      GotoIf(TaggedEqual(value, TheHoleConstant()), if_hole);
    }
    if (IsDoubleElementsKind(to_kind)) {
      if (IsSmiElementsKind(from_kind)) {
        return SmiToFloat64(CAST(value));
      }
      return LoadHeapNumberValue(CAST(value));
    }
    return value;
  }
}

Node* CodeStubAssembler::CalculateNewElementsCapacity(Node* old_capacity,
                                                      ParameterMode mode) {
  CSA_SLOW_ASSERT(this, MatchesParameterMode(old_capacity, mode));
  Node* half_old_capacity = WordOrSmiShr(old_capacity, 1, mode);
  Node* new_capacity = IntPtrOrSmiAdd(half_old_capacity, old_capacity, mode);
  Node* padding =
      IntPtrOrSmiConstant(JSObject::kMinAddedElementsCapacity, mode);
  return IntPtrOrSmiAdd(new_capacity, padding, mode);
}

Node* CodeStubAssembler::TryGrowElementsCapacity(Node* object, Node* elements,
                                                 ElementsKind kind, Node* key,
                                                 Label* bailout) {
  CSA_SLOW_ASSERT(this, TaggedIsNotSmi(object));
  CSA_SLOW_ASSERT(this, IsFixedArrayWithKindOrEmpty(elements, kind));
  CSA_SLOW_ASSERT(this, TaggedIsSmi(key));
  TNode<Smi> capacity = LoadFixedArrayBaseLength(elements);

  ParameterMode mode = OptimalParameterMode();
  return TryGrowElementsCapacity(
      object, elements, kind, TaggedToParameter(key, mode),
      TaggedToParameter(capacity, mode), mode, bailout);
}

Node* CodeStubAssembler::TryGrowElementsCapacity(Node* object, Node* elements,
                                                 ElementsKind kind, Node* key,
                                                 Node* capacity,
                                                 ParameterMode mode,
                                                 Label* bailout) {
  Comment("TryGrowElementsCapacity");
  CSA_SLOW_ASSERT(this, TaggedIsNotSmi(object));
  CSA_SLOW_ASSERT(this, IsFixedArrayWithKindOrEmpty(elements, kind));
  CSA_SLOW_ASSERT(this, MatchesParameterMode(capacity, mode));
  CSA_SLOW_ASSERT(this, MatchesParameterMode(key, mode));

  // If the gap growth is too big, fall back to the runtime.
  Node* max_gap = IntPtrOrSmiConstant(JSObject::kMaxGap, mode);
  Node* max_capacity = IntPtrOrSmiAdd(capacity, max_gap, mode);
  GotoIf(UintPtrOrSmiGreaterThanOrEqual(key, max_capacity, mode), bailout);

  // Calculate the capacity of the new backing store.
  Node* new_capacity = CalculateNewElementsCapacity(
      IntPtrOrSmiAdd(key, IntPtrOrSmiConstant(1, mode), mode), mode);
  return GrowElementsCapacity(object, elements, kind, kind, capacity,
                              new_capacity, mode, bailout);
}

Node* CodeStubAssembler::GrowElementsCapacity(
    Node* object, Node* elements, ElementsKind from_kind, ElementsKind to_kind,
    Node* capacity, Node* new_capacity, ParameterMode mode, Label* bailout) {
  Comment("[ GrowElementsCapacity");
  CSA_SLOW_ASSERT(this, TaggedIsNotSmi(object));
  CSA_SLOW_ASSERT(this, IsFixedArrayWithKindOrEmpty(elements, from_kind));
  CSA_SLOW_ASSERT(this, MatchesParameterMode(capacity, mode));
  CSA_SLOW_ASSERT(this, MatchesParameterMode(new_capacity, mode));

  // If size of the allocation for the new capacity doesn't fit in a page
  // that we can bump-pointer allocate from, fall back to the runtime.
  int max_size = FixedArrayBase::GetMaxLengthForNewSpaceAllocation(to_kind);
  GotoIf(UintPtrOrSmiGreaterThanOrEqual(
             new_capacity, IntPtrOrSmiConstant(max_size, mode), mode),
         bailout);

  // Allocate the new backing store.
  TNode<FixedArrayBase> new_elements =
      AllocateFixedArray(to_kind, new_capacity, mode);

  // Copy the elements from the old elements store to the new.
  // The size-check above guarantees that the |new_elements| is allocated
  // in new space so we can skip the write barrier.
  CopyFixedArrayElements(from_kind, elements, to_kind, new_elements, capacity,
                         new_capacity, SKIP_WRITE_BARRIER, mode);

  StoreObjectField(object, JSObject::kElementsOffset, new_elements);
  Comment("] GrowElementsCapacity");
  return new_elements;
}

void CodeStubAssembler::InitializeAllocationMemento(Node* base,
                                                    Node* base_allocation_size,
                                                    Node* allocation_site) {
  Comment("[Initialize AllocationMemento");
  TNode<HeapObject> memento =
      InnerAllocate(CAST(base), UncheckedCast<IntPtrT>(base_allocation_size));
  StoreMapNoWriteBarrier(memento, RootIndex::kAllocationMementoMap);
  StoreObjectFieldNoWriteBarrier(
      memento, AllocationMemento::kAllocationSiteOffset, allocation_site);
  if (FLAG_allocation_site_pretenuring) {
    TNode<Int32T> count = UncheckedCast<Int32T>(LoadObjectField(
        allocation_site, AllocationSite::kPretenureCreateCountOffset,
        MachineType::Int32()));

    TNode<Int32T> incremented_count = Int32Add(count, Int32Constant(1));
    StoreObjectFieldNoWriteBarrier(
        allocation_site, AllocationSite::kPretenureCreateCountOffset,
        incremented_count, MachineRepresentation::kWord32);
  }
  Comment("]");
}

Node* CodeStubAssembler::TryTaggedToFloat64(Node* value,
                                            Label* if_valueisnotnumber) {
  Label out(this);
  VARIABLE(var_result, MachineRepresentation::kFloat64);

  // Check if the {value} is a Smi or a HeapObject.
  Label if_valueissmi(this), if_valueisnotsmi(this);
  Branch(TaggedIsSmi(value), &if_valueissmi, &if_valueisnotsmi);

  BIND(&if_valueissmi);
  {
    // Convert the Smi {value}.
    var_result.Bind(SmiToFloat64(value));
    Goto(&out);
  }

  BIND(&if_valueisnotsmi);
  {
    // Check if {value} is a HeapNumber.
    Label if_valueisheapnumber(this);
    Branch(IsHeapNumber(value), &if_valueisheapnumber, if_valueisnotnumber);

    BIND(&if_valueisheapnumber);
    {
      // Load the floating point value.
      var_result.Bind(LoadHeapNumberValue(value));
      Goto(&out);
    }
  }
  BIND(&out);
  return var_result.value();
}

Node* CodeStubAssembler::TruncateTaggedToFloat64(Node* context, Node* value) {
  // We might need to loop once due to ToNumber conversion.
  VARIABLE(var_value, MachineRepresentation::kTagged);
  VARIABLE(var_result, MachineRepresentation::kFloat64);
  Label loop(this, &var_value), done_loop(this, &var_result);
  var_value.Bind(value);
  Goto(&loop);
  BIND(&loop);
  {
    Label if_valueisnotnumber(this, Label::kDeferred);

    // Load the current {value}.
    value = var_value.value();

    // Convert {value} to Float64 if it is a number and convert it to a number
    // otherwise.
    Node* const result = TryTaggedToFloat64(value, &if_valueisnotnumber);
    var_result.Bind(result);
    Goto(&done_loop);

    BIND(&if_valueisnotnumber);
    {
      // Convert the {value} to a Number first.
      var_value.Bind(CallBuiltin(Builtins::kNonNumberToNumber, context, value));
      Goto(&loop);
    }
  }
  BIND(&done_loop);
  return var_result.value();
}

Node* CodeStubAssembler::TruncateTaggedToWord32(Node* context, Node* value) {
  VARIABLE(var_result, MachineRepresentation::kWord32);
  Label done(this);
  TaggedToWord32OrBigIntImpl<Object::Conversion::kToNumber>(context, value,
                                                            &done, &var_result);
  BIND(&done);
  return var_result.value();
}

// Truncate {value} to word32 and jump to {if_number} if it is a Number,
// or find that it is a BigInt and jump to {if_bigint}.
void CodeStubAssembler::TaggedToWord32OrBigInt(Node* context, Node* value,
                                               Label* if_number,
                                               Variable* var_word32,
                                               Label* if_bigint,
                                               Variable* var_bigint) {
  TaggedToWord32OrBigIntImpl<Object::Conversion::kToNumeric>(
      context, value, if_number, var_word32, if_bigint, var_bigint);
}

// Truncate {value} to word32 and jump to {if_number} if it is a Number,
// or find that it is a BigInt and jump to {if_bigint}. In either case,
// store the type feedback in {var_feedback}.
void CodeStubAssembler::TaggedToWord32OrBigIntWithFeedback(
    Node* context, Node* value, Label* if_number, Variable* var_word32,
    Label* if_bigint, Variable* var_bigint, Variable* var_feedback) {
  TaggedToWord32OrBigIntImpl<Object::Conversion::kToNumeric>(
      context, value, if_number, var_word32, if_bigint, var_bigint,
      var_feedback);
}

template <Object::Conversion conversion>
void CodeStubAssembler::TaggedToWord32OrBigIntImpl(
    Node* context, Node* value, Label* if_number, Variable* var_word32,
    Label* if_bigint, Variable* var_bigint, Variable* var_feedback) {
  DCHECK(var_word32->rep() == MachineRepresentation::kWord32);
  DCHECK(var_bigint == nullptr ||
         var_bigint->rep() == MachineRepresentation::kTagged);
  DCHECK(var_feedback == nullptr ||
         var_feedback->rep() == MachineRepresentation::kTaggedSigned);

  // We might need to loop after conversion.
  VARIABLE(var_value, MachineRepresentation::kTagged, value);
  OverwriteFeedback(var_feedback, BinaryOperationFeedback::kNone);
  Variable* loop_vars[] = {&var_value, var_feedback};
  int num_vars =
      var_feedback != nullptr ? arraysize(loop_vars) : arraysize(loop_vars) - 1;
  Label loop(this, num_vars, loop_vars);
  Goto(&loop);
  BIND(&loop);
  {
    value = var_value.value();
    Label not_smi(this), is_heap_number(this), is_oddball(this),
        is_bigint(this);
    GotoIf(TaggedIsNotSmi(value), &not_smi);

    // {value} is a Smi.
    var_word32->Bind(SmiToInt32(value));
    CombineFeedback(var_feedback, BinaryOperationFeedback::kSignedSmall);
    Goto(if_number);

    BIND(&not_smi);
    TNode<Map> map = LoadMap(value);
    GotoIf(IsHeapNumberMap(map), &is_heap_number);
    TNode<Uint16T> instance_type = LoadMapInstanceType(map);
    if (conversion == Object::Conversion::kToNumeric) {
      GotoIf(IsBigIntInstanceType(instance_type), &is_bigint);
    }

    // Not HeapNumber (or BigInt if conversion == kToNumeric).
    {
      if (var_feedback != nullptr) {
        // We do not require an Or with earlier feedback here because once we
        // convert the value to a Numeric, we cannot reach this path. We can
        // only reach this path on the first pass when the feedback is kNone.
        CSA_ASSERT(this, SmiEqual(CAST(var_feedback->value()),
                                  SmiConstant(BinaryOperationFeedback::kNone)));
      }
      GotoIf(InstanceTypeEqual(instance_type, ODDBALL_TYPE), &is_oddball);
      // Not an oddball either -> convert.
      auto builtin = conversion == Object::Conversion::kToNumeric
                         ? Builtins::kNonNumberToNumeric
                         : Builtins::kNonNumberToNumber;
      var_value.Bind(CallBuiltin(builtin, context, value));
      OverwriteFeedback(var_feedback, BinaryOperationFeedback::kAny);
      Goto(&loop);

      BIND(&is_oddball);
      var_value.Bind(LoadObjectField(value, Oddball::kToNumberOffset));
      OverwriteFeedback(var_feedback,
                        BinaryOperationFeedback::kNumberOrOddball);
      Goto(&loop);
    }

    BIND(&is_heap_number);
    var_word32->Bind(TruncateHeapNumberValueToWord32(CAST(value)));
    CombineFeedback(var_feedback, BinaryOperationFeedback::kNumber);
    Goto(if_number);

    if (conversion == Object::Conversion::kToNumeric) {
      BIND(&is_bigint);
      var_bigint->Bind(value);
      CombineFeedback(var_feedback, BinaryOperationFeedback::kBigInt);
      Goto(if_bigint);
    }
  }
}

TNode<Int32T> CodeStubAssembler::TruncateHeapNumberValueToWord32(
    TNode<HeapNumber> object) {
  TNode<Float64T> value = LoadHeapNumberValue(object);
  return Signed(TruncateFloat64ToWord32(value));
}

void CodeStubAssembler::TryHeapNumberToSmi(TNode<HeapNumber> number,
                                           TVariable<Smi>& var_result_smi,
                                           Label* if_smi) {
  TNode<Float64T> value = LoadHeapNumberValue(number);
  TryFloat64ToSmi(value, var_result_smi, if_smi);
}

void CodeStubAssembler::TryFloat64ToSmi(TNode<Float64T> value,
                                        TVariable<Smi>& var_result_smi,
                                        Label* if_smi) {
  TNode<Int32T> value32 = RoundFloat64ToInt32(value);
  TNode<Float64T> value64 = ChangeInt32ToFloat64(value32);

  Label if_int32(this), if_heap_number(this, Label::kDeferred);

  GotoIfNot(Float64Equal(value, value64), &if_heap_number);
  GotoIfNot(Word32Equal(value32, Int32Constant(0)), &if_int32);
  Branch(Int32LessThan(UncheckedCast<Int32T>(Float64ExtractHighWord32(value)),
                       Int32Constant(0)),
         &if_heap_number, &if_int32);

  TVARIABLE(Number, var_result);
  BIND(&if_int32);
  {
    if (SmiValuesAre32Bits()) {
      var_result_smi = SmiTag(ChangeInt32ToIntPtr(value32));
    } else {
      DCHECK(SmiValuesAre31Bits());
      TNode<PairT<Int32T, BoolT>> pair = Int32AddWithOverflow(value32, value32);
      TNode<BoolT> overflow = Projection<1>(pair);
      GotoIf(overflow, &if_heap_number);
      var_result_smi =
          BitcastWordToTaggedSigned(ChangeInt32ToIntPtr(Projection<0>(pair)));
    }
    Goto(if_smi);
  }
  BIND(&if_heap_number);
}

TNode<Number> CodeStubAssembler::ChangeFloat64ToTagged(
    SloppyTNode<Float64T> value) {
  Label if_smi(this), done(this);
  TVARIABLE(Smi, var_smi_result);
  TVARIABLE(Number, var_result);
  TryFloat64ToSmi(value, var_smi_result, &if_smi);

  var_result = AllocateHeapNumberWithValue(value);
  Goto(&done);

  BIND(&if_smi);
  {
    var_result = var_smi_result.value();
    Goto(&done);
  }
  BIND(&done);
  return var_result.value();
}

TNode<Number> CodeStubAssembler::ChangeInt32ToTagged(
    SloppyTNode<Int32T> value) {
  if (SmiValuesAre32Bits()) {
    return SmiTag(ChangeInt32ToIntPtr(value));
  }
  DCHECK(SmiValuesAre31Bits());
  TVARIABLE(Number, var_result);
  TNode<PairT<Int32T, BoolT>> pair = Int32AddWithOverflow(value, value);
  TNode<BoolT> overflow = Projection<1>(pair);
  Label if_overflow(this, Label::kDeferred), if_notoverflow(this),
      if_join(this);
  Branch(overflow, &if_overflow, &if_notoverflow);
  BIND(&if_overflow);
  {
    TNode<Float64T> value64 = ChangeInt32ToFloat64(value);
    TNode<HeapNumber> result = AllocateHeapNumberWithValue(value64);
    var_result = result;
    Goto(&if_join);
  }
  BIND(&if_notoverflow);
  {
    TNode<IntPtrT> almost_tagged_value =
        ChangeInt32ToIntPtr(Projection<0>(pair));
    TNode<Smi> result = BitcastWordToTaggedSigned(almost_tagged_value);
    var_result = result;
    Goto(&if_join);
  }
  BIND(&if_join);
  return var_result.value();
}

TNode<Number> CodeStubAssembler::ChangeUint32ToTagged(
    SloppyTNode<Uint32T> value) {
  Label if_overflow(this, Label::kDeferred), if_not_overflow(this),
      if_join(this);
  TVARIABLE(Number, var_result);
  // If {value} > 2^31 - 1, we need to store it in a HeapNumber.
  Branch(Uint32LessThan(Uint32Constant(Smi::kMaxValue), value), &if_overflow,
         &if_not_overflow);

  BIND(&if_not_overflow);
  {
    // The {value} is definitely in valid Smi range.
    var_result = SmiTag(Signed(ChangeUint32ToWord(value)));
  }
  Goto(&if_join);

  BIND(&if_overflow);
  {
    TNode<Float64T> float64_value = ChangeUint32ToFloat64(value);
    var_result = AllocateHeapNumberWithValue(float64_value);
  }
  Goto(&if_join);

  BIND(&if_join);
  return var_result.value();
}

TNode<Number> CodeStubAssembler::ChangeUintPtrToTagged(TNode<UintPtrT> value) {
  Label if_overflow(this, Label::kDeferred), if_not_overflow(this),
      if_join(this);
  TVARIABLE(Number, var_result);
  // If {value} > 2^31 - 1, we need to store it in a HeapNumber.
  Branch(UintPtrLessThan(UintPtrConstant(Smi::kMaxValue), value), &if_overflow,
         &if_not_overflow);

  BIND(&if_not_overflow);
  {
    // The {value} is definitely in valid Smi range.
    var_result = SmiTag(Signed(value));
  }
  Goto(&if_join);

  BIND(&if_overflow);
  {
    TNode<Float64T> float64_value = ChangeUintPtrToFloat64(value);
    var_result = AllocateHeapNumberWithValue(float64_value);
  }
  Goto(&if_join);

  BIND(&if_join);
  return var_result.value();
}

TNode<String> CodeStubAssembler::ToThisString(TNode<Context> context,
                                              TNode<Object> value,
                                              TNode<String> method_name) {
  VARIABLE(var_value, MachineRepresentation::kTagged, value);

  // Check if the {value} is a Smi or a HeapObject.
  Label if_valueissmi(this, Label::kDeferred), if_valueisnotsmi(this),
      if_valueisstring(this);
  Branch(TaggedIsSmi(value), &if_valueissmi, &if_valueisnotsmi);
  BIND(&if_valueisnotsmi);
  {
    // Load the instance type of the {value}.
    TNode<Uint16T> value_instance_type = LoadInstanceType(CAST(value));

    // Check if the {value} is already String.
    Label if_valueisnotstring(this, Label::kDeferred);
    Branch(IsStringInstanceType(value_instance_type), &if_valueisstring,
           &if_valueisnotstring);
    BIND(&if_valueisnotstring);
    {
      // Check if the {value} is null.
      Label if_valueisnullorundefined(this, Label::kDeferred);
      GotoIf(IsNullOrUndefined(value), &if_valueisnullorundefined);
      // Convert the {value} to a String.
      var_value.Bind(CallBuiltin(Builtins::kToString, context, value));
      Goto(&if_valueisstring);

      BIND(&if_valueisnullorundefined);
      {
        // The {value} is either null or undefined.
        ThrowTypeError(context, MessageTemplate::kCalledOnNullOrUndefined,
                       method_name);
      }
    }
  }
  BIND(&if_valueissmi);
  {
    // The {value} is a Smi, convert it to a String.
    var_value.Bind(CallBuiltin(Builtins::kNumberToString, context, value));
    Goto(&if_valueisstring);
  }
  BIND(&if_valueisstring);
  return CAST(var_value.value());
}

TNode<Uint32T> CodeStubAssembler::ChangeNumberToUint32(TNode<Number> value) {
  TVARIABLE(Uint32T, var_result);
  Label if_smi(this), if_heapnumber(this, Label::kDeferred), done(this);
  Branch(TaggedIsSmi(value), &if_smi, &if_heapnumber);
  BIND(&if_smi);
  {
    var_result = Unsigned(SmiToInt32(CAST(value)));
    Goto(&done);
  }
  BIND(&if_heapnumber);
  {
    var_result = ChangeFloat64ToUint32(LoadHeapNumberValue(CAST(value)));
    Goto(&done);
  }
  BIND(&done);
  return var_result.value();
}

TNode<Float64T> CodeStubAssembler::ChangeNumberToFloat64(TNode<Number> value) {
  TVARIABLE(Float64T, result);
  Label smi(this);
  Label done(this, &result);
  GotoIf(TaggedIsSmi(value), &smi);
  result = LoadHeapNumberValue(CAST(value));
  Goto(&done);

  BIND(&smi);
  {
    result = SmiToFloat64(CAST(value));
    Goto(&done);
  }

  BIND(&done);
  return result.value();
}

TNode<UintPtrT> CodeStubAssembler::TryNumberToUintPtr(TNode<Number> value,
                                                      Label* if_negative) {
  TVARIABLE(UintPtrT, result);
  Label done(this, &result);
  Branch(
      TaggedIsSmi(value),
      [&] {
        TNode<Smi> value_smi = CAST(value);
        if (if_negative == nullptr) {
          CSA_SLOW_ASSERT(this, SmiLessThan(SmiConstant(-1), value_smi));
        } else {
          GotoIfNot(TaggedIsPositiveSmi(value), if_negative);
        }
        result = UncheckedCast<UintPtrT>(SmiToIntPtr(value_smi));
        Goto(&done);
      },
      [&] {
        TNode<HeapNumber> value_hn = CAST(value);
        TNode<Float64T> value = LoadHeapNumberValue(value_hn);
        if (if_negative != nullptr) {
          GotoIf(Float64LessThan(value, Float64Constant(0.0)), if_negative);
        }
        result = ChangeFloat64ToUintPtr(value);
        Goto(&done);
      });

  BIND(&done);
  return result.value();
}

TNode<WordT> CodeStubAssembler::TimesSystemPointerSize(
    SloppyTNode<WordT> value) {
  return WordShl(value, kSystemPointerSizeLog2);
}

TNode<WordT> CodeStubAssembler::TimesTaggedSize(SloppyTNode<WordT> value) {
  return WordShl(value, kTaggedSizeLog2);
}

TNode<WordT> CodeStubAssembler::TimesDoubleSize(SloppyTNode<WordT> value) {
  return WordShl(value, kDoubleSizeLog2);
}

TNode<Object> CodeStubAssembler::ToThisValue(TNode<Context> context,
                                             TNode<Object> value,
                                             PrimitiveType primitive_type,
                                             char const* method_name) {
  // We might need to loop once due to JSPrimitiveWrapper unboxing.
  TVARIABLE(Object, var_value, value);
  Label loop(this, &var_value), done_loop(this),
      done_throw(this, Label::kDeferred);
  Goto(&loop);
  BIND(&loop);
  {
    // Check if the {value} is a Smi or a HeapObject.
    GotoIf(
        TaggedIsSmi(var_value.value()),
        (primitive_type == PrimitiveType::kNumber) ? &done_loop : &done_throw);

    TNode<HeapObject> value = CAST(var_value.value());

    // Load the map of the {value}.
    TNode<Map> value_map = LoadMap(value);

    // Load the instance type of the {value}.
    TNode<Uint16T> value_instance_type = LoadMapInstanceType(value_map);

    // Check if {value} is a JSPrimitiveWrapper.
    Label if_valueiswrapper(this, Label::kDeferred), if_valueisnotwrapper(this);
    Branch(InstanceTypeEqual(value_instance_type, JS_PRIMITIVE_WRAPPER_TYPE),
           &if_valueiswrapper, &if_valueisnotwrapper);

    BIND(&if_valueiswrapper);
    {
      // Load the actual value from the {value}.
      var_value = LoadObjectField(value, JSPrimitiveWrapper::kValueOffset);
      Goto(&loop);
    }

    BIND(&if_valueisnotwrapper);
    {
      switch (primitive_type) {
        case PrimitiveType::kBoolean:
          GotoIf(TaggedEqual(value_map, BooleanMapConstant()), &done_loop);
          break;
        case PrimitiveType::kNumber:
          GotoIf(TaggedEqual(value_map, HeapNumberMapConstant()), &done_loop);
          break;
        case PrimitiveType::kString:
          GotoIf(IsStringInstanceType(value_instance_type), &done_loop);
          break;
        case PrimitiveType::kSymbol:
          GotoIf(TaggedEqual(value_map, SymbolMapConstant()), &done_loop);
          break;
      }
      Goto(&done_throw);
    }
  }

  BIND(&done_throw);
  {
    const char* primitive_name = nullptr;
    switch (primitive_type) {
      case PrimitiveType::kBoolean:
        primitive_name = "Boolean";
        break;
      case PrimitiveType::kNumber:
        primitive_name = "Number";
        break;
      case PrimitiveType::kString:
        primitive_name = "String";
        break;
      case PrimitiveType::kSymbol:
        primitive_name = "Symbol";
        break;
    }
    CHECK_NOT_NULL(primitive_name);

    // The {value} is not a compatible receiver for this method.
    ThrowTypeError(context, MessageTemplate::kNotGeneric, method_name,
                   primitive_name);
  }

  BIND(&done_loop);
  return var_value.value();
}

Node* CodeStubAssembler::ThrowIfNotInstanceType(Node* context, Node* value,
                                                InstanceType instance_type,
                                                char const* method_name) {
  Label out(this), throw_exception(this, Label::kDeferred);
  VARIABLE(var_value_map, MachineRepresentation::kTagged);

  GotoIf(TaggedIsSmi(value), &throw_exception);

  // Load the instance type of the {value}.
  var_value_map.Bind(LoadMap(value));
  TNode<Uint16T> const value_instance_type =
      LoadMapInstanceType(var_value_map.value());

  Branch(Word32Equal(value_instance_type, Int32Constant(instance_type)), &out,
         &throw_exception);

  // The {value} is not a compatible receiver for this method.
  BIND(&throw_exception);
  ThrowTypeError(context, MessageTemplate::kIncompatibleMethodReceiver,
                 StringConstant(method_name), value);

  BIND(&out);
  return var_value_map.value();
}

void CodeStubAssembler::ThrowIfNotJSReceiver(TNode<Context> context,
                                             TNode<Object> value,
                                             MessageTemplate msg_template,
                                             const char* method_name) {
  Label done(this), throw_exception(this, Label::kDeferred);

  GotoIf(TaggedIsSmi(value), &throw_exception);

  // Load the instance type of the {value}.
  TNode<Map> value_map = LoadMap(CAST(value));
  TNode<Uint16T> const value_instance_type = LoadMapInstanceType(value_map);

  Branch(IsJSReceiverInstanceType(value_instance_type), &done,
         &throw_exception);

  // The {value} is not a compatible receiver for this method.
  BIND(&throw_exception);
  ThrowTypeError(context, msg_template, StringConstant(method_name), value);

  BIND(&done);
}

void CodeStubAssembler::ThrowIfNotCallable(TNode<Context> context,
                                           TNode<Object> value,
                                           const char* method_name) {
  Label out(this), throw_exception(this, Label::kDeferred);

  GotoIf(TaggedIsSmi(value), &throw_exception);
  Branch(IsCallable(CAST(value)), &out, &throw_exception);

  // The {value} is not a compatible receiver for this method.
  BIND(&throw_exception);
  ThrowTypeError(context, MessageTemplate::kCalledNonCallable, method_name);

  BIND(&out);
}

void CodeStubAssembler::ThrowRangeError(Node* context, MessageTemplate message,
                                        Node* arg0, Node* arg1, Node* arg2) {
  TNode<Smi> template_index = SmiConstant(static_cast<int>(message));
  if (arg0 == nullptr) {
    CallRuntime(Runtime::kThrowRangeError, context, template_index);
  } else if (arg1 == nullptr) {
    CallRuntime(Runtime::kThrowRangeError, context, template_index, arg0);
  } else if (arg2 == nullptr) {
    CallRuntime(Runtime::kThrowRangeError, context, template_index, arg0, arg1);
  } else {
    CallRuntime(Runtime::kThrowRangeError, context, template_index, arg0, arg1,
                arg2);
  }
  Unreachable();
}

void CodeStubAssembler::ThrowTypeError(Node* context, MessageTemplate message,
                                       char const* arg0, char const* arg1) {
  Node* arg0_node = nullptr;
  if (arg0) arg0_node = StringConstant(arg0);
  Node* arg1_node = nullptr;
  if (arg1) arg1_node = StringConstant(arg1);
  ThrowTypeError(context, message, arg0_node, arg1_node);
}

void CodeStubAssembler::ThrowTypeError(Node* context, MessageTemplate message,
                                       Node* arg0, Node* arg1, Node* arg2) {
  TNode<Smi> template_index = SmiConstant(static_cast<int>(message));
  if (arg0 == nullptr) {
    CallRuntime(Runtime::kThrowTypeError, context, template_index);
  } else if (arg1 == nullptr) {
    CallRuntime(Runtime::kThrowTypeError, context, template_index, arg0);
  } else if (arg2 == nullptr) {
    CallRuntime(Runtime::kThrowTypeError, context, template_index, arg0, arg1);
  } else {
    CallRuntime(Runtime::kThrowTypeError, context, template_index, arg0, arg1,
                arg2);
  }
  Unreachable();
}

TNode<BoolT> CodeStubAssembler::InstanceTypeEqual(
    SloppyTNode<Int32T> instance_type, int type) {
  return Word32Equal(instance_type, Int32Constant(type));
}

TNode<BoolT> CodeStubAssembler::IsDictionaryMap(SloppyTNode<Map> map) {
  CSA_SLOW_ASSERT(this, IsMap(map));
  return IsSetWord32<Map::IsDictionaryMapBit>(LoadMapBitField3(map));
}

TNode<BoolT> CodeStubAssembler::IsExtensibleMap(SloppyTNode<Map> map) {
  CSA_ASSERT(this, IsMap(map));
  return IsSetWord32<Map::IsExtensibleBit>(LoadMapBitField3(map));
}

TNode<BoolT> CodeStubAssembler::IsExtensibleNonPrototypeMap(TNode<Map> map) {
  int kMask = Map::IsExtensibleBit::kMask | Map::IsPrototypeMapBit::kMask;
  int kExpected = Map::IsExtensibleBit::kMask;
  return Word32Equal(Word32And(LoadMapBitField3(map), Int32Constant(kMask)),
                     Int32Constant(kExpected));
}

TNode<BoolT> CodeStubAssembler::IsCallableMap(SloppyTNode<Map> map) {
  CSA_ASSERT(this, IsMap(map));
  return IsSetWord32<Map::IsCallableBit>(LoadMapBitField(map));
}

TNode<BoolT> CodeStubAssembler::IsDebugInfo(TNode<HeapObject> object) {
  return HasInstanceType(object, DEBUG_INFO_TYPE);
}

TNode<BoolT> CodeStubAssembler::IsDeprecatedMap(SloppyTNode<Map> map) {
  CSA_ASSERT(this, IsMap(map));
  return IsSetWord32<Map::IsDeprecatedBit>(LoadMapBitField3(map));
}

TNode<BoolT> CodeStubAssembler::IsUndetectableMap(SloppyTNode<Map> map) {
  CSA_ASSERT(this, IsMap(map));
  return IsSetWord32<Map::IsUndetectableBit>(LoadMapBitField(map));
}

TNode<BoolT> CodeStubAssembler::IsNoElementsProtectorCellInvalid() {
  TNode<Smi> invalid = SmiConstant(Isolate::kProtectorInvalid);
  TNode<PropertyCell> cell = NoElementsProtectorConstant();
  TNode<Object> cell_value = LoadObjectField(cell, PropertyCell::kValueOffset);
  return TaggedEqual(cell_value, invalid);
}

TNode<BoolT> CodeStubAssembler::IsArrayIteratorProtectorCellInvalid() {
  TNode<Smi> invalid = SmiConstant(Isolate::kProtectorInvalid);
  TNode<PropertyCell> cell = ArrayIteratorProtectorConstant();
  TNode<Object> cell_value = LoadObjectField(cell, PropertyCell::kValueOffset);
  return TaggedEqual(cell_value, invalid);
}

TNode<BoolT> CodeStubAssembler::IsPromiseResolveProtectorCellInvalid() {
  TNode<Smi> invalid = SmiConstant(Isolate::kProtectorInvalid);
  TNode<Cell> cell = PromiseResolveProtectorConstant();
  TNode<Object> cell_value = LoadObjectField(cell, Cell::kValueOffset);
  return TaggedEqual(cell_value, invalid);
}

TNode<BoolT> CodeStubAssembler::IsPromiseThenProtectorCellInvalid() {
  TNode<Smi> invalid = SmiConstant(Isolate::kProtectorInvalid);
  TNode<PropertyCell> cell = PromiseThenProtectorConstant();
  TNode<Object> cell_value = LoadObjectField(cell, PropertyCell::kValueOffset);
  return TaggedEqual(cell_value, invalid);
}

TNode<BoolT> CodeStubAssembler::IsArraySpeciesProtectorCellInvalid() {
  TNode<Smi> invalid = SmiConstant(Isolate::kProtectorInvalid);
  TNode<PropertyCell> cell = ArraySpeciesProtectorConstant();
  TNode<Object> cell_value = LoadObjectField(cell, PropertyCell::kValueOffset);
  return TaggedEqual(cell_value, invalid);
}

TNode<BoolT> CodeStubAssembler::IsTypedArraySpeciesProtectorCellInvalid() {
  TNode<Smi> invalid = SmiConstant(Isolate::kProtectorInvalid);
  TNode<PropertyCell> cell = TypedArraySpeciesProtectorConstant();
  TNode<Object> cell_value = LoadObjectField(cell, PropertyCell::kValueOffset);
  return TaggedEqual(cell_value, invalid);
}

TNode<BoolT> CodeStubAssembler::IsRegExpSpeciesProtectorCellInvalid(
    TNode<NativeContext> native_context) {
  TNode<PropertyCell> cell = CAST(LoadContextElement(
      native_context, Context::REGEXP_SPECIES_PROTECTOR_INDEX));
  TNode<Object> cell_value = LoadObjectField(cell, PropertyCell::kValueOffset);
  TNode<Smi> invalid = SmiConstant(Isolate::kProtectorInvalid);
  return TaggedEqual(cell_value, invalid);
}

TNode<BoolT> CodeStubAssembler::IsPromiseSpeciesProtectorCellInvalid() {
  TNode<Smi> invalid = SmiConstant(Isolate::kProtectorInvalid);
  TNode<PropertyCell> cell = PromiseSpeciesProtectorConstant();
  TNode<Object> cell_value = LoadObjectField(cell, PropertyCell::kValueOffset);
  return TaggedEqual(cell_value, invalid);
}

TNode<BoolT> CodeStubAssembler::IsPrototypeInitialArrayPrototype(
    SloppyTNode<Context> context, SloppyTNode<Map> map) {
  TNode<NativeContext> const native_context = LoadNativeContext(context);
  TNode<Object> const initial_array_prototype = LoadContextElement(
      native_context, Context::INITIAL_ARRAY_PROTOTYPE_INDEX);
  TNode<HeapObject> proto = LoadMapPrototype(map);
  return TaggedEqual(proto, initial_array_prototype);
}

TNode<BoolT> CodeStubAssembler::IsPrototypeTypedArrayPrototype(
    SloppyTNode<Context> context, SloppyTNode<Map> map) {
  TNode<NativeContext> const native_context = LoadNativeContext(context);
  TNode<Object> const typed_array_prototype =
      LoadContextElement(native_context, Context::TYPED_ARRAY_PROTOTYPE_INDEX);
  TNode<HeapObject> proto = LoadMapPrototype(map);
  TNode<HeapObject> proto_of_proto = Select<HeapObject>(
      IsJSObject(proto), [=] { return LoadMapPrototype(LoadMap(proto)); },
      [=] { return NullConstant(); });
  return TaggedEqual(proto_of_proto, typed_array_prototype);
}

TNode<BoolT> CodeStubAssembler::IsFastAliasedArgumentsMap(
    TNode<Context> context, TNode<Map> map) {
  TNode<NativeContext> const native_context = LoadNativeContext(context);
  TNode<Object> const arguments_map = LoadContextElement(
      native_context, Context::FAST_ALIASED_ARGUMENTS_MAP_INDEX);
  return TaggedEqual(arguments_map, map);
}

TNode<BoolT> CodeStubAssembler::IsSlowAliasedArgumentsMap(
    TNode<Context> context, TNode<Map> map) {
  TNode<NativeContext> const native_context = LoadNativeContext(context);
  TNode<Object> const arguments_map = LoadContextElement(
      native_context, Context::SLOW_ALIASED_ARGUMENTS_MAP_INDEX);
  return TaggedEqual(arguments_map, map);
}

TNode<BoolT> CodeStubAssembler::IsSloppyArgumentsMap(TNode<Context> context,
                                                     TNode<Map> map) {
  TNode<NativeContext> const native_context = LoadNativeContext(context);
  TNode<Object> const arguments_map =
      LoadContextElement(native_context, Context::SLOPPY_ARGUMENTS_MAP_INDEX);
  return TaggedEqual(arguments_map, map);
}

TNode<BoolT> CodeStubAssembler::IsStrictArgumentsMap(TNode<Context> context,
                                                     TNode<Map> map) {
  TNode<NativeContext> const native_context = LoadNativeContext(context);
  TNode<Object> const arguments_map =
      LoadContextElement(native_context, Context::STRICT_ARGUMENTS_MAP_INDEX);
  return TaggedEqual(arguments_map, map);
}

TNode<BoolT> CodeStubAssembler::TaggedIsCallable(TNode<Object> object) {
  return Select<BoolT>(
      TaggedIsSmi(object), [=] { return Int32FalseConstant(); },
      [=] {
        return IsCallableMap(LoadMap(UncheckedCast<HeapObject>(object)));
      });
}

TNode<BoolT> CodeStubAssembler::IsCallable(SloppyTNode<HeapObject> object) {
  return IsCallableMap(LoadMap(object));
}

TNode<BoolT> CodeStubAssembler::IsCell(SloppyTNode<HeapObject> object) {
  return TaggedEqual(LoadMap(object), CellMapConstant());
}

TNode<BoolT> CodeStubAssembler::IsCode(SloppyTNode<HeapObject> object) {
  return HasInstanceType(object, CODE_TYPE);
}

TNode<BoolT> CodeStubAssembler::IsConstructorMap(SloppyTNode<Map> map) {
  CSA_ASSERT(this, IsMap(map));
  return IsSetWord32<Map::IsConstructorBit>(LoadMapBitField(map));
}

TNode<BoolT> CodeStubAssembler::IsConstructor(SloppyTNode<HeapObject> object) {
  return IsConstructorMap(LoadMap(object));
}

TNode<BoolT> CodeStubAssembler::IsFunctionWithPrototypeSlotMap(
    SloppyTNode<Map> map) {
  CSA_ASSERT(this, IsMap(map));
  return IsSetWord32<Map::HasPrototypeSlotBit>(LoadMapBitField(map));
}

TNode<BoolT> CodeStubAssembler::IsSpecialReceiverInstanceType(
    TNode<Int32T> instance_type) {
  STATIC_ASSERT(JS_GLOBAL_OBJECT_TYPE <= LAST_SPECIAL_RECEIVER_TYPE);
  return Int32LessThanOrEqual(instance_type,
                              Int32Constant(LAST_SPECIAL_RECEIVER_TYPE));
}

TNode<BoolT> CodeStubAssembler::IsCustomElementsReceiverInstanceType(
    TNode<Int32T> instance_type) {
  return Int32LessThanOrEqual(instance_type,
                              Int32Constant(LAST_CUSTOM_ELEMENTS_RECEIVER));
}

TNode<BoolT> CodeStubAssembler::IsStringInstanceType(
    SloppyTNode<Int32T> instance_type) {
  STATIC_ASSERT(INTERNALIZED_STRING_TYPE == FIRST_TYPE);
  return Int32LessThan(instance_type, Int32Constant(FIRST_NONSTRING_TYPE));
}

TNode<BoolT> CodeStubAssembler::IsOneByteStringInstanceType(
    SloppyTNode<Int32T> instance_type) {
  CSA_ASSERT(this, IsStringInstanceType(instance_type));
  return Word32Equal(
      Word32And(instance_type, Int32Constant(kStringEncodingMask)),
      Int32Constant(kOneByteStringTag));
}

TNode<BoolT> CodeStubAssembler::IsSequentialStringInstanceType(
    SloppyTNode<Int32T> instance_type) {
  CSA_ASSERT(this, IsStringInstanceType(instance_type));
  return Word32Equal(
      Word32And(instance_type, Int32Constant(kStringRepresentationMask)),
      Int32Constant(kSeqStringTag));
}

TNode<BoolT> CodeStubAssembler::IsConsStringInstanceType(
    SloppyTNode<Int32T> instance_type) {
  CSA_ASSERT(this, IsStringInstanceType(instance_type));
  return Word32Equal(
      Word32And(instance_type, Int32Constant(kStringRepresentationMask)),
      Int32Constant(kConsStringTag));
}

TNode<BoolT> CodeStubAssembler::IsIndirectStringInstanceType(
    SloppyTNode<Int32T> instance_type) {
  CSA_ASSERT(this, IsStringInstanceType(instance_type));
  STATIC_ASSERT(kIsIndirectStringMask == 0x1);
  STATIC_ASSERT(kIsIndirectStringTag == 0x1);
  return UncheckedCast<BoolT>(
      Word32And(instance_type, Int32Constant(kIsIndirectStringMask)));
}

TNode<BoolT> CodeStubAssembler::IsExternalStringInstanceType(
    SloppyTNode<Int32T> instance_type) {
  CSA_ASSERT(this, IsStringInstanceType(instance_type));
  return Word32Equal(
      Word32And(instance_type, Int32Constant(kStringRepresentationMask)),
      Int32Constant(kExternalStringTag));
}

TNode<BoolT> CodeStubAssembler::IsUncachedExternalStringInstanceType(
    SloppyTNode<Int32T> instance_type) {
  CSA_ASSERT(this, IsStringInstanceType(instance_type));
  STATIC_ASSERT(kUncachedExternalStringTag != 0);
  return IsSetWord32(instance_type, kUncachedExternalStringMask);
}

TNode<BoolT> CodeStubAssembler::IsJSReceiverInstanceType(
    SloppyTNode<Int32T> instance_type) {
  STATIC_ASSERT(LAST_JS_RECEIVER_TYPE == LAST_TYPE);
  return Int32GreaterThanOrEqual(instance_type,
                                 Int32Constant(FIRST_JS_RECEIVER_TYPE));
}

TNode<BoolT> CodeStubAssembler::IsJSReceiverMap(SloppyTNode<Map> map) {
  return IsJSReceiverInstanceType(LoadMapInstanceType(map));
}

TNode<BoolT> CodeStubAssembler::IsJSReceiver(SloppyTNode<HeapObject> object) {
  return IsJSReceiverMap(LoadMap(object));
}

TNode<BoolT> CodeStubAssembler::IsNullOrJSReceiver(
    SloppyTNode<HeapObject> object) {
  return UncheckedCast<BoolT>(Word32Or(IsJSReceiver(object), IsNull(object)));
}

TNode<BoolT> CodeStubAssembler::IsNullOrUndefined(SloppyTNode<Object> value) {
  return UncheckedCast<BoolT>(Word32Or(IsUndefined(value), IsNull(value)));
}

TNode<BoolT> CodeStubAssembler::IsJSGlobalProxyInstanceType(
    SloppyTNode<Int32T> instance_type) {
  return InstanceTypeEqual(instance_type, JS_GLOBAL_PROXY_TYPE);
}

TNode<BoolT> CodeStubAssembler::IsJSGlobalProxyMap(SloppyTNode<Map> map) {
  return IsJSGlobalProxyInstanceType(LoadMapInstanceType(map));
}

TNode<BoolT> CodeStubAssembler::IsJSGlobalProxy(
    SloppyTNode<HeapObject> object) {
  return IsJSGlobalProxyMap(LoadMap(object));
}

TNode<BoolT> CodeStubAssembler::IsJSObjectInstanceType(
    SloppyTNode<Int32T> instance_type) {
  STATIC_ASSERT(LAST_JS_OBJECT_TYPE == LAST_TYPE);
  return Int32GreaterThanOrEqual(instance_type,
                                 Int32Constant(FIRST_JS_OBJECT_TYPE));
}

TNode<BoolT> CodeStubAssembler::IsJSObjectMap(SloppyTNode<Map> map) {
  CSA_ASSERT(this, IsMap(map));
  return IsJSObjectInstanceType(LoadMapInstanceType(map));
}

TNode<BoolT> CodeStubAssembler::IsJSObject(SloppyTNode<HeapObject> object) {
  return IsJSObjectMap(LoadMap(object));
}

TNode<BoolT> CodeStubAssembler::IsJSPromiseMap(SloppyTNode<Map> map) {
  CSA_ASSERT(this, IsMap(map));
  return InstanceTypeEqual(LoadMapInstanceType(map), JS_PROMISE_TYPE);
}

TNode<BoolT> CodeStubAssembler::IsJSPromise(SloppyTNode<HeapObject> object) {
  return IsJSPromiseMap(LoadMap(object));
}

TNode<BoolT> CodeStubAssembler::IsJSProxy(SloppyTNode<HeapObject> object) {
  return HasInstanceType(object, JS_PROXY_TYPE);
}

TNode<BoolT> CodeStubAssembler::IsJSStringIterator(
    SloppyTNode<HeapObject> object) {
  return HasInstanceType(object, JS_STRING_ITERATOR_TYPE);
}

TNode<BoolT> CodeStubAssembler::IsMap(SloppyTNode<HeapObject> map) {
  return IsMetaMap(LoadMap(map));
}

TNode<BoolT> CodeStubAssembler::IsJSPrimitiveWrapperInstanceType(
    SloppyTNode<Int32T> instance_type) {
  return InstanceTypeEqual(instance_type, JS_PRIMITIVE_WRAPPER_TYPE);
}

TNode<BoolT> CodeStubAssembler::IsJSPrimitiveWrapper(
    SloppyTNode<HeapObject> object) {
  return IsJSPrimitiveWrapperMap(LoadMap(object));
}

TNode<BoolT> CodeStubAssembler::IsJSPrimitiveWrapperMap(SloppyTNode<Map> map) {
  return IsJSPrimitiveWrapperInstanceType(LoadMapInstanceType(map));
}

TNode<BoolT> CodeStubAssembler::IsJSArrayInstanceType(
    SloppyTNode<Int32T> instance_type) {
  return InstanceTypeEqual(instance_type, JS_ARRAY_TYPE);
}

TNode<BoolT> CodeStubAssembler::IsJSArray(SloppyTNode<HeapObject> object) {
  return IsJSArrayMap(LoadMap(object));
}

TNode<BoolT> CodeStubAssembler::IsJSArrayMap(SloppyTNode<Map> map) {
  return IsJSArrayInstanceType(LoadMapInstanceType(map));
}

TNode<BoolT> CodeStubAssembler::IsJSArrayIterator(
    SloppyTNode<HeapObject> object) {
  return HasInstanceType(object, JS_ARRAY_ITERATOR_TYPE);
}

TNode<BoolT> CodeStubAssembler::IsJSAsyncGeneratorObject(
    SloppyTNode<HeapObject> object) {
  return HasInstanceType(object, JS_ASYNC_GENERATOR_OBJECT_TYPE);
}

TNode<BoolT> CodeStubAssembler::IsContext(SloppyTNode<HeapObject> object) {
  TNode<Uint16T> instance_type = LoadInstanceType(object);
  return UncheckedCast<BoolT>(Word32And(
      Int32GreaterThanOrEqual(instance_type, Int32Constant(FIRST_CONTEXT_TYPE)),
      Int32LessThanOrEqual(instance_type, Int32Constant(LAST_CONTEXT_TYPE))));
}

TNode<BoolT> CodeStubAssembler::IsFixedArray(SloppyTNode<HeapObject> object) {
  return HasInstanceType(object, FIXED_ARRAY_TYPE);
}

TNode<BoolT> CodeStubAssembler::IsFixedArraySubclass(
    SloppyTNode<HeapObject> object) {
  TNode<Uint16T> instance_type = LoadInstanceType(object);
  return UncheckedCast<BoolT>(
      Word32And(Int32GreaterThanOrEqual(instance_type,
                                        Int32Constant(FIRST_FIXED_ARRAY_TYPE)),
                Int32LessThanOrEqual(instance_type,
                                     Int32Constant(LAST_FIXED_ARRAY_TYPE))));
}

TNode<BoolT> CodeStubAssembler::IsNotWeakFixedArraySubclass(
    SloppyTNode<HeapObject> object) {
  TNode<Uint16T> instance_type = LoadInstanceType(object);
  return UncheckedCast<BoolT>(Word32Or(
      Int32LessThan(instance_type, Int32Constant(FIRST_WEAK_FIXED_ARRAY_TYPE)),
      Int32GreaterThan(instance_type,
                       Int32Constant(LAST_WEAK_FIXED_ARRAY_TYPE))));
}

TNode<BoolT> CodeStubAssembler::IsPromiseCapability(
    SloppyTNode<HeapObject> object) {
  return HasInstanceType(object, PROMISE_CAPABILITY_TYPE);
}

TNode<BoolT> CodeStubAssembler::IsPropertyArray(
    SloppyTNode<HeapObject> object) {
  return HasInstanceType(object, PROPERTY_ARRAY_TYPE);
}

// This complicated check is due to elements oddities. If a smi array is empty
// after Array.p.shift, it is replaced by the empty array constant. If it is
// later filled with a double element, we try to grow it but pass in a double
// elements kind. Usually this would cause a size mismatch (since the source
// fixed array has HOLEY_ELEMENTS and destination has
// HOLEY_DOUBLE_ELEMENTS), but we don't have to worry about it when the
// source array is empty.
// TODO(jgruber): It might we worth creating an empty_double_array constant to
// simplify this case.
TNode<BoolT> CodeStubAssembler::IsFixedArrayWithKindOrEmpty(
    SloppyTNode<HeapObject> object, ElementsKind kind) {
  Label out(this);
  TVARIABLE(BoolT, var_result, Int32TrueConstant());

  GotoIf(IsFixedArrayWithKind(object, kind), &out);

  TNode<Smi> const length = LoadFixedArrayBaseLength(CAST(object));
  GotoIf(SmiEqual(length, SmiConstant(0)), &out);

  var_result = Int32FalseConstant();
  Goto(&out);

  BIND(&out);
  return var_result.value();
}

TNode<BoolT> CodeStubAssembler::IsFixedArrayWithKind(
    SloppyTNode<HeapObject> object, ElementsKind kind) {
  if (IsDoubleElementsKind(kind)) {
    return IsFixedDoubleArray(object);
  } else {
    DCHECK(IsSmiOrObjectElementsKind(kind) || IsSealedElementsKind(kind) ||
           IsNonextensibleElementsKind(kind));
    return IsFixedArraySubclass(object);
  }
}

TNode<BoolT> CodeStubAssembler::IsBoolean(SloppyTNode<HeapObject> object) {
  return IsBooleanMap(LoadMap(object));
}

TNode<BoolT> CodeStubAssembler::IsPropertyCell(SloppyTNode<HeapObject> object) {
  return IsPropertyCellMap(LoadMap(object));
}

TNode<BoolT> CodeStubAssembler::IsAccessorInfo(SloppyTNode<HeapObject> object) {
  return IsAccessorInfoMap(LoadMap(object));
}

TNode<BoolT> CodeStubAssembler::IsAccessorPair(SloppyTNode<HeapObject> object) {
  return IsAccessorPairMap(LoadMap(object));
}

TNode<BoolT> CodeStubAssembler::IsAllocationSite(
    SloppyTNode<HeapObject> object) {
  return IsAllocationSiteInstanceType(LoadInstanceType(object));
}

TNode<BoolT> CodeStubAssembler::IsHeapNumber(SloppyTNode<HeapObject> object) {
  return IsHeapNumberMap(LoadMap(object));
}

TNode<BoolT> CodeStubAssembler::IsHeapNumberInstanceType(
    SloppyTNode<Int32T> instance_type) {
  return InstanceTypeEqual(instance_type, HEAP_NUMBER_TYPE);
}

TNode<BoolT> CodeStubAssembler::IsOddball(SloppyTNode<HeapObject> object) {
  return IsOddballInstanceType(LoadInstanceType(object));
}

TNode<BoolT> CodeStubAssembler::IsOddballInstanceType(
    SloppyTNode<Int32T> instance_type) {
  return InstanceTypeEqual(instance_type, ODDBALL_TYPE);
}

TNode<BoolT> CodeStubAssembler::IsFeedbackCell(SloppyTNode<HeapObject> object) {
  return HasInstanceType(object, FEEDBACK_CELL_TYPE);
}

TNode<BoolT> CodeStubAssembler::IsFeedbackVector(
    SloppyTNode<HeapObject> object) {
  return IsFeedbackVectorMap(LoadMap(object));
}

TNode<BoolT> CodeStubAssembler::IsName(SloppyTNode<HeapObject> object) {
  return IsNameInstanceType(LoadInstanceType(object));
}

TNode<BoolT> CodeStubAssembler::IsNameInstanceType(
    SloppyTNode<Int32T> instance_type) {
  return Int32LessThanOrEqual(instance_type, Int32Constant(LAST_NAME_TYPE));
}

TNode<BoolT> CodeStubAssembler::IsString(SloppyTNode<HeapObject> object) {
  return IsStringInstanceType(LoadInstanceType(object));
}

TNode<BoolT> CodeStubAssembler::IsSymbolInstanceType(
    SloppyTNode<Int32T> instance_type) {
  return InstanceTypeEqual(instance_type, SYMBOL_TYPE);
}

TNode<BoolT> CodeStubAssembler::IsSymbol(SloppyTNode<HeapObject> object) {
  return IsSymbolMap(LoadMap(object));
}

TNode<BoolT> CodeStubAssembler::IsInternalizedStringInstanceType(
    TNode<Int32T> instance_type) {
  STATIC_ASSERT(kNotInternalizedTag != 0);
  return Word32Equal(
      Word32And(instance_type,
                Int32Constant(kIsNotStringMask | kIsNotInternalizedMask)),
      Int32Constant(kStringTag | kInternalizedTag));
}

TNode<BoolT> CodeStubAssembler::IsUniqueName(TNode<HeapObject> object) {
  TNode<Uint16T> instance_type = LoadInstanceType(object);
  return Select<BoolT>(
      IsInternalizedStringInstanceType(instance_type),
      [=] { return Int32TrueConstant(); },
      [=] { return IsSymbolInstanceType(instance_type); });
}

TNode<BoolT> CodeStubAssembler::IsUniqueNameNoIndex(TNode<HeapObject> object) {
  TNode<Uint16T> instance_type = LoadInstanceType(object);
  return Select<BoolT>(
      IsInternalizedStringInstanceType(instance_type),
      [=] {
        return IsSetWord32(LoadNameHashField(CAST(object)),
                           Name::kIsNotArrayIndexMask);
      },
      [=] { return IsSymbolInstanceType(instance_type); });
}

TNode<BoolT> CodeStubAssembler::IsBigIntInstanceType(
    SloppyTNode<Int32T> instance_type) {
  return InstanceTypeEqual(instance_type, BIGINT_TYPE);
}

TNode<BoolT> CodeStubAssembler::IsBigInt(SloppyTNode<HeapObject> object) {
  return IsBigIntInstanceType(LoadInstanceType(object));
}

TNode<BoolT> CodeStubAssembler::IsPrimitiveInstanceType(
    SloppyTNode<Int32T> instance_type) {
  return Int32LessThanOrEqual(instance_type,
                              Int32Constant(LAST_PRIMITIVE_TYPE));
}

TNode<BoolT> CodeStubAssembler::IsPrivateSymbol(
    SloppyTNode<HeapObject> object) {
  return Select<BoolT>(
      IsSymbol(object),
      [=] {
        TNode<Symbol> symbol = CAST(object);
        TNode<Uint32T> flags =
            LoadObjectField<Uint32T>(symbol, Symbol::kFlagsOffset);
        return IsSetWord32<Symbol::IsPrivateBit>(flags);
      },
      [=] { return Int32FalseConstant(); });
}

TNode<BoolT> CodeStubAssembler::IsPrivateName(SloppyTNode<Symbol> symbol) {
  TNode<Uint32T> flags = LoadObjectField<Uint32T>(symbol, Symbol::kFlagsOffset);
  return IsSetWord32<Symbol::IsPrivateNameBit>(flags);
}

TNode<BoolT> CodeStubAssembler::IsNativeContext(
    SloppyTNode<HeapObject> object) {
  return TaggedEqual(LoadMap(object), NativeContextMapConstant());
}

TNode<BoolT> CodeStubAssembler::IsFixedDoubleArray(
    SloppyTNode<HeapObject> object) {
  return TaggedEqual(LoadMap(object), FixedDoubleArrayMapConstant());
}

TNode<BoolT> CodeStubAssembler::IsHashTable(SloppyTNode<HeapObject> object) {
  TNode<Uint16T> instance_type = LoadInstanceType(object);
  return UncheckedCast<BoolT>(
      Word32And(Int32GreaterThanOrEqual(instance_type,
                                        Int32Constant(FIRST_HASH_TABLE_TYPE)),
                Int32LessThanOrEqual(instance_type,
                                     Int32Constant(LAST_HASH_TABLE_TYPE))));
}

TNode<BoolT> CodeStubAssembler::IsEphemeronHashTable(
    SloppyTNode<HeapObject> object) {
  return HasInstanceType(object, EPHEMERON_HASH_TABLE_TYPE);
}

TNode<BoolT> CodeStubAssembler::IsNameDictionary(
    SloppyTNode<HeapObject> object) {
  return HasInstanceType(object, NAME_DICTIONARY_TYPE);
}

TNode<BoolT> CodeStubAssembler::IsGlobalDictionary(
    SloppyTNode<HeapObject> object) {
  return HasInstanceType(object, GLOBAL_DICTIONARY_TYPE);
}

TNode<BoolT> CodeStubAssembler::IsNumberDictionary(
    SloppyTNode<HeapObject> object) {
  return HasInstanceType(object, NUMBER_DICTIONARY_TYPE);
}

TNode<BoolT> CodeStubAssembler::IsJSGeneratorObject(
    SloppyTNode<HeapObject> object) {
  return HasInstanceType(object, JS_GENERATOR_OBJECT_TYPE);
}

TNode<BoolT> CodeStubAssembler::IsJSFunctionInstanceType(
    SloppyTNode<Int32T> instance_type) {
  return InstanceTypeEqual(instance_type, JS_FUNCTION_TYPE);
}

TNode<BoolT> CodeStubAssembler::IsAllocationSiteInstanceType(
    SloppyTNode<Int32T> instance_type) {
  return InstanceTypeEqual(instance_type, ALLOCATION_SITE_TYPE);
}

TNode<BoolT> CodeStubAssembler::IsJSFunction(SloppyTNode<HeapObject> object) {
  return IsJSFunctionMap(LoadMap(object));
}

TNode<BoolT> CodeStubAssembler::IsJSFunctionMap(SloppyTNode<Map> map) {
  return IsJSFunctionInstanceType(LoadMapInstanceType(map));
}

TNode<BoolT> CodeStubAssembler::IsJSTypedArrayInstanceType(
    SloppyTNode<Int32T> instance_type) {
  return InstanceTypeEqual(instance_type, JS_TYPED_ARRAY_TYPE);
}

TNode<BoolT> CodeStubAssembler::IsJSTypedArrayMap(SloppyTNode<Map> map) {
  return IsJSTypedArrayInstanceType(LoadMapInstanceType(map));
}

TNode<BoolT> CodeStubAssembler::IsJSTypedArray(SloppyTNode<HeapObject> object) {
  return IsJSTypedArrayMap(LoadMap(object));
}

TNode<BoolT> CodeStubAssembler::IsJSArrayBuffer(
    SloppyTNode<HeapObject> object) {
  return HasInstanceType(object, JS_ARRAY_BUFFER_TYPE);
}

TNode<BoolT> CodeStubAssembler::IsJSDataView(TNode<HeapObject> object) {
  return HasInstanceType(object, JS_DATA_VIEW_TYPE);
}

TNode<BoolT> CodeStubAssembler::IsJSRegExp(SloppyTNode<HeapObject> object) {
  return HasInstanceType(object, JS_REGEXP_TYPE);
}

TNode<BoolT> CodeStubAssembler::IsNumber(SloppyTNode<Object> object) {
  return Select<BoolT>(
      TaggedIsSmi(object), [=] { return Int32TrueConstant(); },
      [=] { return IsHeapNumber(CAST(object)); });
}

TNode<BoolT> CodeStubAssembler::IsNumeric(SloppyTNode<Object> object) {
  return Select<BoolT>(
      TaggedIsSmi(object), [=] { return Int32TrueConstant(); },
      [=] {
        return UncheckedCast<BoolT>(
            Word32Or(IsHeapNumber(CAST(object)), IsBigInt(CAST(object))));
      });
}

TNode<BoolT> CodeStubAssembler::IsNumberNormalized(SloppyTNode<Number> number) {
  TVARIABLE(BoolT, var_result, Int32TrueConstant());
  Label out(this);

  GotoIf(TaggedIsSmi(number), &out);

  TNode<Float64T> value = LoadHeapNumberValue(CAST(number));
  TNode<Float64T> smi_min =
      Float64Constant(static_cast<double>(Smi::kMinValue));
  TNode<Float64T> smi_max =
      Float64Constant(static_cast<double>(Smi::kMaxValue));

  GotoIf(Float64LessThan(value, smi_min), &out);
  GotoIf(Float64GreaterThan(value, smi_max), &out);
  GotoIfNot(Float64Equal(value, value), &out);  // NaN.

  var_result = Int32FalseConstant();
  Goto(&out);

  BIND(&out);
  return var_result.value();
}

TNode<BoolT> CodeStubAssembler::IsNumberPositive(SloppyTNode<Number> number) {
  return Select<BoolT>(
      TaggedIsSmi(number), [=] { return TaggedIsPositiveSmi(number); },
      [=] { return IsHeapNumberPositive(CAST(number)); });
}

// TODO(cbruni): Use TNode<HeapNumber> instead of custom name.
TNode<BoolT> CodeStubAssembler::IsHeapNumberPositive(TNode<HeapNumber> number) {
  TNode<Float64T> value = LoadHeapNumberValue(number);
  TNode<Float64T> float_zero = Float64Constant(0.);
  return Float64GreaterThanOrEqual(value, float_zero);
}

TNode<BoolT> CodeStubAssembler::IsNumberNonNegativeSafeInteger(
    TNode<Number> number) {
  return Select<BoolT>(
      // TODO(cbruni): Introduce TaggedIsNonNegateSmi to avoid confusion.
      TaggedIsSmi(number), [=] { return TaggedIsPositiveSmi(number); },
      [=] {
        TNode<HeapNumber> heap_number = CAST(number);
        return Select<BoolT>(
            IsInteger(heap_number),
            [=] { return IsHeapNumberPositive(heap_number); },
            [=] { return Int32FalseConstant(); });
      });
}

TNode<BoolT> CodeStubAssembler::IsSafeInteger(TNode<Object> number) {
  return Select<BoolT>(
      TaggedIsSmi(number), [=] { return Int32TrueConstant(); },
      [=] {
        return Select<BoolT>(
            IsHeapNumber(CAST(number)),
            [=] { return IsSafeInteger(UncheckedCast<HeapNumber>(number)); },
            [=] { return Int32FalseConstant(); });
      });
}

TNode<BoolT> CodeStubAssembler::IsSafeInteger(TNode<HeapNumber> number) {
  // Load the actual value of {number}.
  TNode<Float64T> number_value = LoadHeapNumberValue(number);
  // Truncate the value of {number} to an integer (or an infinity).
  TNode<Float64T> integer = Float64Trunc(number_value);

  return Select<BoolT>(
      // Check if {number}s value matches the integer (ruling out the
      // infinities).
      Float64Equal(Float64Sub(number_value, integer), Float64Constant(0.0)),
      [=] {
        // Check if the {integer} value is in safe integer range.
        return Float64LessThanOrEqual(Float64Abs(integer),
                                      Float64Constant(kMaxSafeInteger));
      },
      [=] { return Int32FalseConstant(); });
}

TNode<BoolT> CodeStubAssembler::IsInteger(TNode<Object> number) {
  return Select<BoolT>(
      TaggedIsSmi(number), [=] { return Int32TrueConstant(); },
      [=] {
        return Select<BoolT>(
            IsHeapNumber(CAST(number)),
            [=] { return IsInteger(UncheckedCast<HeapNumber>(number)); },
            [=] { return Int32FalseConstant(); });
      });
}

TNode<BoolT> CodeStubAssembler::IsInteger(TNode<HeapNumber> number) {
  TNode<Float64T> number_value = LoadHeapNumberValue(number);
  // Truncate the value of {number} to an integer (or an infinity).
  TNode<Float64T> integer = Float64Trunc(number_value);
  // Check if {number}s value matches the integer (ruling out the infinities).
  return Float64Equal(Float64Sub(number_value, integer), Float64Constant(0.0));
}

TNode<BoolT> CodeStubAssembler::IsHeapNumberUint32(TNode<HeapNumber> number) {
  // Check that the HeapNumber is a valid uint32
  return Select<BoolT>(
      IsHeapNumberPositive(number),
      [=] {
        TNode<Float64T> value = LoadHeapNumberValue(number);
        TNode<Uint32T> int_value = TruncateFloat64ToWord32(value);
        return Float64Equal(value, ChangeUint32ToFloat64(int_value));
      },
      [=] { return Int32FalseConstant(); });
}

TNode<BoolT> CodeStubAssembler::IsNumberArrayIndex(TNode<Number> number) {
  return Select<BoolT>(
      TaggedIsSmi(number), [=] { return TaggedIsPositiveSmi(number); },
      [=] { return IsHeapNumberUint32(CAST(number)); });
}

Node* CodeStubAssembler::FixedArraySizeDoesntFitInNewSpace(Node* element_count,
                                                           int base_size,
                                                           ParameterMode mode) {
  int max_newspace_elements =
      (kMaxRegularHeapObjectSize - base_size) / kTaggedSize;
  return IntPtrOrSmiGreaterThan(
      element_count, IntPtrOrSmiConstant(max_newspace_elements, mode), mode);
}

TNode<Int32T> CodeStubAssembler::StringCharCodeAt(SloppyTNode<String> string,
                                                  SloppyTNode<IntPtrT> index) {
  CSA_ASSERT(this, IsString(string));

  CSA_ASSERT(this, IntPtrGreaterThanOrEqual(index, IntPtrConstant(0)));
  CSA_ASSERT(this, IntPtrLessThan(index, LoadStringLengthAsWord(string)));

  TVARIABLE(Int32T, var_result);

  Label return_result(this), if_runtime(this, Label::kDeferred),
      if_stringistwobyte(this), if_stringisonebyte(this);

  ToDirectStringAssembler to_direct(state(), string);
  to_direct.TryToDirect(&if_runtime);
  TNode<IntPtrT> const offset = IntPtrAdd(index, to_direct.offset());
  TNode<Int32T> const instance_type = to_direct.instance_type();
  TNode<RawPtrT> const string_data = to_direct.PointerToData(&if_runtime);

  // Check if the {string} is a TwoByteSeqString or a OneByteSeqString.
  Branch(IsOneByteStringInstanceType(instance_type), &if_stringisonebyte,
         &if_stringistwobyte);

  BIND(&if_stringisonebyte);
  {
    var_result =
        UncheckedCast<Int32T>(Load(MachineType::Uint8(), string_data, offset));
    Goto(&return_result);
  }

  BIND(&if_stringistwobyte);
  {
    var_result =
        UncheckedCast<Int32T>(Load(MachineType::Uint16(), string_data,
                                   WordShl(offset, IntPtrConstant(1))));
    Goto(&return_result);
  }

  BIND(&if_runtime);
  {
    TNode<Object> result = CallRuntime(
        Runtime::kStringCharCodeAt, NoContextConstant(), string, SmiTag(index));
    var_result = SmiToInt32(CAST(result));
    Goto(&return_result);
  }

  BIND(&return_result);
  return var_result.value();
}

TNode<String> CodeStubAssembler::StringFromSingleCharCode(TNode<Int32T> code) {
  VARIABLE(var_result, MachineRepresentation::kTagged);

  // Check if the {code} is a one-byte char code.
  Label if_codeisonebyte(this), if_codeistwobyte(this, Label::kDeferred),
      if_done(this);
  Branch(Int32LessThanOrEqual(code, Int32Constant(String::kMaxOneByteCharCode)),
         &if_codeisonebyte, &if_codeistwobyte);
  BIND(&if_codeisonebyte);
  {
    // Load the isolate wide single character string cache.
    TNode<FixedArray> cache = SingleCharacterStringCacheConstant();
    TNode<IntPtrT> code_index = Signed(ChangeUint32ToWord(code));

    // Check if we have an entry for the {code} in the single character string
    // cache already.
    Label if_entryisundefined(this, Label::kDeferred),
        if_entryisnotundefined(this);
    TNode<Object> entry = UnsafeLoadFixedArrayElement(cache, code_index);
    Branch(IsUndefined(entry), &if_entryisundefined, &if_entryisnotundefined);

    BIND(&if_entryisundefined);
    {
      // Allocate a new SeqOneByteString for {code} and store it in the {cache}.
      TNode<String> result = AllocateSeqOneByteString(1);
      StoreNoWriteBarrier(
          MachineRepresentation::kWord8, result,
          IntPtrConstant(SeqOneByteString::kHeaderSize - kHeapObjectTag), code);
      StoreFixedArrayElement(cache, code_index, result);
      var_result.Bind(result);
      Goto(&if_done);
    }

    BIND(&if_entryisnotundefined);
    {
      // Return the entry from the {cache}.
      var_result.Bind(entry);
      Goto(&if_done);
    }
  }

  BIND(&if_codeistwobyte);
  {
    // Allocate a new SeqTwoByteString for {code}.
    TNode<String> result = AllocateSeqTwoByteString(1);
    StoreNoWriteBarrier(
        MachineRepresentation::kWord16, result,
        IntPtrConstant(SeqTwoByteString::kHeaderSize - kHeapObjectTag), code);
    var_result.Bind(result);
    Goto(&if_done);
  }

  BIND(&if_done);
  CSA_ASSERT(this, IsString(var_result.value()));
  return CAST(var_result.value());
}

// A wrapper around CopyStringCharacters which determines the correct string
// encoding, allocates a corresponding sequential string, and then copies the
// given character range using CopyStringCharacters.
// |from_string| must be a sequential string.
// 0 <= |from_index| <= |from_index| + |character_count| < from_string.length.
TNode<String> CodeStubAssembler::AllocAndCopyStringCharacters(
    Node* from, Node* from_instance_type, TNode<IntPtrT> from_index,
    TNode<IntPtrT> character_count) {
  Label end(this), one_byte_sequential(this), two_byte_sequential(this);
  TVARIABLE(String, var_result);

  Branch(IsOneByteStringInstanceType(from_instance_type), &one_byte_sequential,
         &two_byte_sequential);

  // The subject string is a sequential one-byte string.
  BIND(&one_byte_sequential);
  {
    TNode<String> result = AllocateSeqOneByteString(
        Unsigned(TruncateIntPtrToInt32(character_count)));
    CopyStringCharacters(from, result, from_index, IntPtrConstant(0),
                         character_count, String::ONE_BYTE_ENCODING,
                         String::ONE_BYTE_ENCODING);
    var_result = result;
    Goto(&end);
  }

  // The subject string is a sequential two-byte string.
  BIND(&two_byte_sequential);
  {
    TNode<String> result = AllocateSeqTwoByteString(
        Unsigned(TruncateIntPtrToInt32(character_count)));
    CopyStringCharacters(from, result, from_index, IntPtrConstant(0),
                         character_count, String::TWO_BYTE_ENCODING,
                         String::TWO_BYTE_ENCODING);
    var_result = result;
    Goto(&end);
  }

  BIND(&end);
  return var_result.value();
}

TNode<String> CodeStubAssembler::SubString(TNode<String> string,
                                           TNode<IntPtrT> from,
                                           TNode<IntPtrT> to) {
  TVARIABLE(String, var_result);
  ToDirectStringAssembler to_direct(state(), string);
  Label end(this), runtime(this);

  TNode<IntPtrT> const substr_length = IntPtrSub(to, from);
  TNode<IntPtrT> const string_length = LoadStringLengthAsWord(string);

  // Begin dispatching based on substring length.

  Label original_string_or_invalid_length(this);
  GotoIf(UintPtrGreaterThanOrEqual(substr_length, string_length),
         &original_string_or_invalid_length);

  // A real substring (substr_length < string_length).
  Label empty(this);
  GotoIf(IntPtrEqual(substr_length, IntPtrConstant(0)), &empty);

  Label single_char(this);
  GotoIf(IntPtrEqual(substr_length, IntPtrConstant(1)), &single_char);

  // Deal with different string types: update the index if necessary
  // and extract the underlying string.

  TNode<String> direct_string = to_direct.TryToDirect(&runtime);
  TNode<IntPtrT> offset = IntPtrAdd(from, to_direct.offset());
  TNode<Int32T> const instance_type = to_direct.instance_type();

  // The subject string can only be external or sequential string of either
  // encoding at this point.
  Label external_string(this);
  {
    if (FLAG_string_slices) {
      Label next(this);

      // Short slice.  Copy instead of slicing.
      GotoIf(IntPtrLessThan(substr_length,
                            IntPtrConstant(SlicedString::kMinLength)),
             &next);

      // Allocate new sliced string.

      Counters* counters = isolate()->counters();
      IncrementCounter(counters->sub_string_native(), 1);

      Label one_byte_slice(this), two_byte_slice(this);
      Branch(IsOneByteStringInstanceType(to_direct.instance_type()),
             &one_byte_slice, &two_byte_slice);

      BIND(&one_byte_slice);
      {
        var_result = AllocateSlicedOneByteString(
            Unsigned(TruncateIntPtrToInt32(substr_length)), direct_string,
            SmiTag(offset));
        Goto(&end);
      }

      BIND(&two_byte_slice);
      {
        var_result = AllocateSlicedTwoByteString(
            Unsigned(TruncateIntPtrToInt32(substr_length)), direct_string,
            SmiTag(offset));
        Goto(&end);
      }

      BIND(&next);
    }

    // The subject string can only be external or sequential string of either
    // encoding at this point.
    GotoIf(to_direct.is_external(), &external_string);

    var_result = AllocAndCopyStringCharacters(direct_string, instance_type,
                                              offset, substr_length);

    Counters* counters = isolate()->counters();
    IncrementCounter(counters->sub_string_native(), 1);

    Goto(&end);
  }

  // Handle external string.
  BIND(&external_string);
  {
    TNode<RawPtrT> const fake_sequential_string =
        to_direct.PointerToString(&runtime);

    var_result = AllocAndCopyStringCharacters(
        fake_sequential_string, instance_type, offset, substr_length);

    Counters* counters = isolate()->counters();
    IncrementCounter(counters->sub_string_native(), 1);

    Goto(&end);
  }

  BIND(&empty);
  {
    var_result = EmptyStringConstant();
    Goto(&end);
  }

  // Substrings of length 1 are generated through CharCodeAt and FromCharCode.
  BIND(&single_char);
  {
    TNode<Int32T> char_code = StringCharCodeAt(string, from);
    var_result = StringFromSingleCharCode(char_code);
    Goto(&end);
  }

  BIND(&original_string_or_invalid_length);
  {
    CSA_ASSERT(this, IntPtrEqual(substr_length, string_length));

    // Equal length - check if {from, to} == {0, str.length}.
    GotoIf(UintPtrGreaterThan(from, IntPtrConstant(0)), &runtime);

    // Return the original string (substr_length == string_length).

    Counters* counters = isolate()->counters();
    IncrementCounter(counters->sub_string_native(), 1);

    var_result = string;
    Goto(&end);
  }

  // Fall back to a runtime call.
  BIND(&runtime);
  {
    var_result =
        CAST(CallRuntime(Runtime::kStringSubstring, NoContextConstant(), string,
                         SmiTag(from), SmiTag(to)));
    Goto(&end);
  }

  BIND(&end);
  return var_result.value();
}

ToDirectStringAssembler::ToDirectStringAssembler(
    compiler::CodeAssemblerState* state, TNode<String> string, Flags flags)
    : CodeStubAssembler(state),
      var_string_(string, this),
      var_instance_type_(LoadInstanceType(string), this),
      var_offset_(IntPtrConstant(0), this),
      var_is_external_(Int32Constant(0), this),
      flags_(flags) {}

TNode<String> ToDirectStringAssembler::TryToDirect(Label* if_bailout) {
  VariableList vars({&var_string_, &var_offset_, &var_instance_type_}, zone());
  Label dispatch(this, vars);
  Label if_iscons(this);
  Label if_isexternal(this);
  Label if_issliced(this);
  Label if_isthin(this);
  Label out(this);

  Branch(IsSequentialStringInstanceType(var_instance_type_.value()), &out,
         &dispatch);

  // Dispatch based on string representation.
  BIND(&dispatch);
  {
    int32_t values[] = {
        kSeqStringTag,    kConsStringTag, kExternalStringTag,
        kSlicedStringTag, kThinStringTag,
    };
    Label* labels[] = {
        &out, &if_iscons, &if_isexternal, &if_issliced, &if_isthin,
    };
    STATIC_ASSERT(arraysize(values) == arraysize(labels));

    TNode<Int32T> const representation = Word32And(
        var_instance_type_.value(), Int32Constant(kStringRepresentationMask));
    Switch(representation, if_bailout, values, labels, arraysize(values));
  }

  // Cons string.  Check whether it is flat, then fetch first part.
  // Flat cons strings have an empty second part.
  BIND(&if_iscons);
  {
    TNode<String> const string = var_string_.value();
    GotoIfNot(IsEmptyString(
                  LoadObjectField<String>(string, ConsString::kSecondOffset)),
              if_bailout);

    TNode<String> const lhs =
        LoadObjectField<String>(string, ConsString::kFirstOffset);
    var_string_ = lhs;
    var_instance_type_ = LoadInstanceType(lhs);

    Goto(&dispatch);
  }

  // Sliced string. Fetch parent and correct start index by offset.
  BIND(&if_issliced);
  {
    if (!FLAG_string_slices || (flags_ & kDontUnpackSlicedStrings)) {
      Goto(if_bailout);
    } else {
      TNode<String> const string = var_string_.value();
      TNode<IntPtrT> const sliced_offset =
          LoadAndUntagObjectField(string, SlicedString::kOffsetOffset);
      var_offset_ = IntPtrAdd(var_offset_.value(), sliced_offset);

      TNode<String> const parent =
          LoadObjectField<String>(string, SlicedString::kParentOffset);
      var_string_ = parent;
      var_instance_type_ = LoadInstanceType(parent);

      Goto(&dispatch);
    }
  }

  // Thin string. Fetch the actual string.
  BIND(&if_isthin);
  {
    TNode<String> const string = var_string_.value();
    TNode<String> const actual_string =
        LoadObjectField<String>(string, ThinString::kActualOffset);
    TNode<Uint16T> const actual_instance_type = LoadInstanceType(actual_string);

    var_string_ = actual_string;
    var_instance_type_ = actual_instance_type;

    Goto(&dispatch);
  }

  // External string.
  BIND(&if_isexternal);
  var_is_external_ = Int32Constant(1);
  Goto(&out);

  BIND(&out);
  return var_string_.value();
}

TNode<RawPtrT> ToDirectStringAssembler::TryToSequential(
    StringPointerKind ptr_kind, Label* if_bailout) {
  CHECK(ptr_kind == PTR_TO_DATA || ptr_kind == PTR_TO_STRING);

  TVARIABLE(RawPtrT, var_result);
  Label out(this), if_issequential(this), if_isexternal(this, Label::kDeferred);
  Branch(is_external(), &if_isexternal, &if_issequential);

  BIND(&if_issequential);
  {
    STATIC_ASSERT(SeqOneByteString::kHeaderSize ==
                  SeqTwoByteString::kHeaderSize);
    TNode<IntPtrT> result = BitcastTaggedToWord(var_string_.value());
    if (ptr_kind == PTR_TO_DATA) {
      result = IntPtrAdd(result, IntPtrConstant(SeqOneByteString::kHeaderSize -
                                                kHeapObjectTag));
    }
    var_result = ReinterpretCast<RawPtrT>(result);
    Goto(&out);
  }

  BIND(&if_isexternal);
  {
    GotoIf(IsUncachedExternalStringInstanceType(var_instance_type_.value()),
           if_bailout);

    TNode<String> string = var_string_.value();
    TNode<IntPtrT> result =
        LoadObjectField<IntPtrT>(string, ExternalString::kResourceDataOffset);
    if (ptr_kind == PTR_TO_STRING) {
      result = IntPtrSub(result, IntPtrConstant(SeqOneByteString::kHeaderSize -
                                                kHeapObjectTag));
    }
    var_result = ReinterpretCast<RawPtrT>(result);
    Goto(&out);
  }

  BIND(&out);
  return var_result.value();
}

void CodeStubAssembler::BranchIfCanDerefIndirectString(
    TNode<String> string, TNode<Int32T> instance_type, Label* can_deref,
    Label* cannot_deref) {
  TNode<Int32T> representation =
      Word32And(instance_type, Int32Constant(kStringRepresentationMask));
  GotoIf(Word32Equal(representation, Int32Constant(kThinStringTag)), can_deref);
  GotoIf(Word32NotEqual(representation, Int32Constant(kConsStringTag)),
         cannot_deref);
  // Cons string.
  TNode<String> rhs =
      LoadObjectField<String>(string, ConsString::kSecondOffset);
  GotoIf(IsEmptyString(rhs), can_deref);
  Goto(cannot_deref);
}

TNode<String> CodeStubAssembler::DerefIndirectString(
    TNode<String> string, TNode<Int32T> instance_type, Label* cannot_deref) {
  Label deref(this);
  BranchIfCanDerefIndirectString(string, instance_type, &deref, cannot_deref);
  BIND(&deref);
  STATIC_ASSERT(static_cast<int>(ThinString::kActualOffset) ==
                static_cast<int>(ConsString::kFirstOffset));
  return LoadObjectField<String>(string, ThinString::kActualOffset);
}

void CodeStubAssembler::DerefIndirectString(TVariable<String>* var_string,
                                            TNode<Int32T> instance_type) {
#ifdef DEBUG
  Label can_deref(this), cannot_deref(this);
  BranchIfCanDerefIndirectString(var_string->value(), instance_type, &can_deref,
                                 &cannot_deref);
  BIND(&cannot_deref);
  DebugBreak();  // Should be able to dereference string.
  Goto(&can_deref);
  BIND(&can_deref);
#endif  // DEBUG

  STATIC_ASSERT(static_cast<int>(ThinString::kActualOffset) ==
                static_cast<int>(ConsString::kFirstOffset));
  *var_string =
      LoadObjectField<String>(var_string->value(), ThinString::kActualOffset);
}

void CodeStubAssembler::MaybeDerefIndirectString(TVariable<String>* var_string,
                                                 TNode<Int32T> instance_type,
                                                 Label* did_deref,
                                                 Label* cannot_deref) {
  Label deref(this);
  BranchIfCanDerefIndirectString(var_string->value(), instance_type, &deref,
                                 cannot_deref);

  BIND(&deref);
  {
    DerefIndirectString(var_string, instance_type);
    Goto(did_deref);
  }
}

void CodeStubAssembler::MaybeDerefIndirectStrings(
    TVariable<String>* var_left, TNode<Int32T> left_instance_type,
    TVariable<String>* var_right, TNode<Int32T> right_instance_type,
    Label* did_something) {
  Label did_nothing_left(this), did_something_left(this),
      didnt_do_anything(this);
  MaybeDerefIndirectString(var_left, left_instance_type, &did_something_left,
                           &did_nothing_left);

  BIND(&did_something_left);
  {
    MaybeDerefIndirectString(var_right, right_instance_type, did_something,
                             did_something);
  }

  BIND(&did_nothing_left);
  {
    MaybeDerefIndirectString(var_right, right_instance_type, did_something,
                             &didnt_do_anything);
  }

  BIND(&didnt_do_anything);
  // Fall through if neither string was an indirect string.
}

TNode<String> CodeStubAssembler::StringAdd(Node* context, TNode<String> left,
                                           TNode<String> right) {
  TVARIABLE(String, result);
  Label check_right(this), runtime(this, Label::kDeferred), cons(this),
      done(this, &result), done_native(this, &result);
  Counters* counters = isolate()->counters();

  TNode<Uint32T> left_length = LoadStringLengthAsWord32(left);
  GotoIfNot(Word32Equal(left_length, Uint32Constant(0)), &check_right);
  result = right;
  Goto(&done_native);

  BIND(&check_right);
  TNode<Uint32T> right_length = LoadStringLengthAsWord32(right);
  GotoIfNot(Word32Equal(right_length, Uint32Constant(0)), &cons);
  result = left;
  Goto(&done_native);

  BIND(&cons);
  {
    TNode<Uint32T> new_length = Uint32Add(left_length, right_length);

    // If new length is greater than String::kMaxLength, goto runtime to
    // throw. Note: we also need to invalidate the string length protector, so
    // can't just throw here directly.
    GotoIf(Uint32GreaterThan(new_length, Uint32Constant(String::kMaxLength)),
           &runtime);

    TVARIABLE(String, var_left, left);
    TVARIABLE(String, var_right, right);
    Variable* input_vars[2] = {&var_left, &var_right};
    Label non_cons(this, 2, input_vars);
    Label slow(this, Label::kDeferred);
    GotoIf(Uint32LessThan(new_length, Uint32Constant(ConsString::kMinLength)),
           &non_cons);

    result =
        AllocateConsString(new_length, var_left.value(), var_right.value());
    Goto(&done_native);

    BIND(&non_cons);

    Comment("Full string concatenate");
    TNode<Int32T> left_instance_type = LoadInstanceType(var_left.value());
    TNode<Int32T> right_instance_type = LoadInstanceType(var_right.value());
    // Compute intersection and difference of instance types.

    TNode<Int32T> ored_instance_types =
        Word32Or(left_instance_type, right_instance_type);
    TNode<Word32T> xored_instance_types =
        Word32Xor(left_instance_type, right_instance_type);

    // Check if both strings have the same encoding and both are sequential.
    GotoIf(IsSetWord32(xored_instance_types, kStringEncodingMask), &runtime);
    GotoIf(IsSetWord32(ored_instance_types, kStringRepresentationMask), &slow);

    TNode<IntPtrT> word_left_length = Signed(ChangeUint32ToWord(left_length));
    TNode<IntPtrT> word_right_length = Signed(ChangeUint32ToWord(right_length));

    Label two_byte(this);
    GotoIf(Word32Equal(Word32And(ored_instance_types,
                                 Int32Constant(kStringEncodingMask)),
                       Int32Constant(kTwoByteStringTag)),
           &two_byte);
    // One-byte sequential string case
    result = AllocateSeqOneByteString(new_length);
    CopyStringCharacters(var_left.value(), result.value(), IntPtrConstant(0),
                         IntPtrConstant(0), word_left_length,
                         String::ONE_BYTE_ENCODING, String::ONE_BYTE_ENCODING);
    CopyStringCharacters(var_right.value(), result.value(), IntPtrConstant(0),
                         word_left_length, word_right_length,
                         String::ONE_BYTE_ENCODING, String::ONE_BYTE_ENCODING);
    Goto(&done_native);

    BIND(&two_byte);
    {
      // Two-byte sequential string case
      result = AllocateSeqTwoByteString(new_length);
      CopyStringCharacters(var_left.value(), result.value(), IntPtrConstant(0),
                           IntPtrConstant(0), word_left_length,
                           String::TWO_BYTE_ENCODING,
                           String::TWO_BYTE_ENCODING);
      CopyStringCharacters(var_right.value(), result.value(), IntPtrConstant(0),
                           word_left_length, word_right_length,
                           String::TWO_BYTE_ENCODING,
                           String::TWO_BYTE_ENCODING);
      Goto(&done_native);
    }

    BIND(&slow);
    {
      // Try to unwrap indirect strings, restart the above attempt on success.
      MaybeDerefIndirectStrings(&var_left, left_instance_type, &var_right,
                                right_instance_type, &non_cons);
      Goto(&runtime);
    }
  }
  BIND(&runtime);
  {
    result = CAST(CallRuntime(Runtime::kStringAdd, context, left, right));
    Goto(&done);
  }

  BIND(&done_native);
  {
    IncrementCounter(counters->string_add_native(), 1);
    Goto(&done);
  }

  BIND(&done);
  return result.value();
}

TNode<String> CodeStubAssembler::StringFromSingleUTF16EncodedCodePoint(
    TNode<Int32T> codepoint) {
  VARIABLE(var_result, MachineRepresentation::kTagged, EmptyStringConstant());

  Label if_isword16(this), if_isword32(this), return_result(this);

  Branch(Uint32LessThan(codepoint, Int32Constant(0x10000)), &if_isword16,
         &if_isword32);

  BIND(&if_isword16);
  {
    var_result.Bind(StringFromSingleCharCode(codepoint));
    Goto(&return_result);
  }

  BIND(&if_isword32);
  {
    TNode<String> value = AllocateSeqTwoByteString(2);
    StoreNoWriteBarrier(
        MachineRepresentation::kWord32, value,
        IntPtrConstant(SeqTwoByteString::kHeaderSize - kHeapObjectTag),
        codepoint);
    var_result.Bind(value);
    Goto(&return_result);
  }

  BIND(&return_result);
  return CAST(var_result.value());
}

TNode<Number> CodeStubAssembler::StringToNumber(TNode<String> input) {
  Label runtime(this, Label::kDeferred);
  Label end(this);

  TVARIABLE(Number, var_result);

  // Check if string has a cached array index.
  TNode<Uint32T> hash = LoadNameHashField(input);
  GotoIf(IsSetWord32(hash, Name::kDoesNotContainCachedArrayIndexMask),
         &runtime);

  var_result =
      SmiTag(Signed(DecodeWordFromWord32<String::ArrayIndexValueBits>(hash)));
  Goto(&end);

  BIND(&runtime);
  {
    var_result =
        CAST(CallRuntime(Runtime::kStringToNumber, NoContextConstant(), input));
    Goto(&end);
  }

  BIND(&end);
  return var_result.value();
}

TNode<String> CodeStubAssembler::NumberToString(TNode<Number> input) {
  TVARIABLE(String, result);
  TVARIABLE(Smi, smi_input);
  Label runtime(this, Label::kDeferred), if_smi(this), if_heap_number(this),
      done(this, &result);

  // Load the number string cache.
  TNode<FixedArray> number_string_cache = NumberStringCacheConstant();

  // Make the hash mask from the length of the number string cache. It
  // contains two elements (number and string) for each cache entry.
  // TODO(ishell): cleanup mask handling.
  TNode<IntPtrT> mask =
      BitcastTaggedSignedToWord(LoadFixedArrayBaseLength(number_string_cache));
  TNode<IntPtrT> one = IntPtrConstant(1);
  mask = IntPtrSub(mask, one);

  GotoIfNot(TaggedIsSmi(input), &if_heap_number);
  smi_input = CAST(input);
  Goto(&if_smi);

  BIND(&if_heap_number);
  {
    Comment("NumberToString - HeapNumber");
    TNode<HeapNumber> heap_number_input = CAST(input);
    // Try normalizing the HeapNumber.
    TryHeapNumberToSmi(heap_number_input, smi_input, &if_smi);

    // Make a hash from the two 32-bit values of the double.
    TNode<Int32T> low =
        LoadObjectField<Int32T>(heap_number_input, HeapNumber::kValueOffset);
    TNode<Int32T> high = LoadObjectField<Int32T>(
        heap_number_input, HeapNumber::kValueOffset + kIntSize);
    TNode<Word32T> hash = Word32Xor(low, high);
    TNode<IntPtrT> word_hash = WordShl(ChangeInt32ToIntPtr(hash), one);
    TNode<WordT> index =
        WordAnd(word_hash, WordSar(mask, SmiShiftBitsConstant()));

    // Cache entry's key must be a heap number
    TNode<Object> number_key =
        UnsafeLoadFixedArrayElement(number_string_cache, index);
    GotoIf(TaggedIsSmi(number_key), &runtime);
    TNode<HeapObject> number_key_heap_object = CAST(number_key);
    GotoIfNot(IsHeapNumber(number_key_heap_object), &runtime);

    // Cache entry's key must match the heap number value we're looking for.
    TNode<Int32T> low_compare = LoadObjectField<Int32T>(
        number_key_heap_object, HeapNumber::kValueOffset);
    TNode<Int32T> high_compare = LoadObjectField<Int32T>(
        number_key_heap_object, HeapNumber::kValueOffset + kIntSize);
    GotoIfNot(Word32Equal(low, low_compare), &runtime);
    GotoIfNot(Word32Equal(high, high_compare), &runtime);

    // Heap number match, return value from cache entry.
    result = CAST(
        UnsafeLoadFixedArrayElement(number_string_cache, index, kTaggedSize));
    Goto(&done);
  }

  BIND(&if_smi);
  {
    Comment("NumberToString - Smi");
    // Load the smi key, make sure it matches the smi we're looking for.
    TNode<Object> smi_index = BitcastWordToTagged(WordAnd(
        WordShl(BitcastTaggedSignedToWord(smi_input.value()), one), mask));
    TNode<Object> smi_key = UnsafeLoadFixedArrayElement(
        number_string_cache, smi_index, 0, SMI_PARAMETERS);
    GotoIf(TaggedNotEqual(smi_key, smi_input.value()), &runtime);

    // Smi match, return value from cache entry.
    result = CAST(UnsafeLoadFixedArrayElement(number_string_cache, smi_index,
                                              kTaggedSize, SMI_PARAMETERS));
    Goto(&done);
  }

  BIND(&runtime);
  {
    // No cache entry, go to the runtime.
    result =
        CAST(CallRuntime(Runtime::kNumberToString, NoContextConstant(), input));
    Goto(&done);
  }
  BIND(&done);
  return result.value();
}

Node* CodeStubAssembler::NonNumberToNumberOrNumeric(
    Node* context, Node* input, Object::Conversion mode,
    BigIntHandling bigint_handling) {
  CSA_ASSERT(this, Word32BinaryNot(TaggedIsSmi(input)));
  CSA_ASSERT(this, Word32BinaryNot(IsHeapNumber(input)));

  // We might need to loop once here due to ToPrimitive conversions.
  VARIABLE(var_input, MachineRepresentation::kTagged, input);
  VARIABLE(var_result, MachineRepresentation::kTagged);
  Label loop(this, &var_input);
  Label end(this);
  Goto(&loop);
  BIND(&loop);
  {
    // Load the current {input} value (known to be a HeapObject).
    Node* input = var_input.value();

    // Dispatch on the {input} instance type.
    TNode<Uint16T> input_instance_type = LoadInstanceType(input);
    Label if_inputisstring(this), if_inputisoddball(this),
        if_inputisbigint(this), if_inputisreceiver(this, Label::kDeferred),
        if_inputisother(this, Label::kDeferred);
    GotoIf(IsStringInstanceType(input_instance_type), &if_inputisstring);
    GotoIf(IsBigIntInstanceType(input_instance_type), &if_inputisbigint);
    GotoIf(InstanceTypeEqual(input_instance_type, ODDBALL_TYPE),
           &if_inputisoddball);
    Branch(IsJSReceiverInstanceType(input_instance_type), &if_inputisreceiver,
           &if_inputisother);

    BIND(&if_inputisstring);
    {
      // The {input} is a String, use the fast stub to convert it to a Number.
      TNode<String> string_input = CAST(input);
      var_result.Bind(StringToNumber(string_input));
      Goto(&end);
    }

    BIND(&if_inputisbigint);
    if (mode == Object::Conversion::kToNumeric) {
      var_result.Bind(input);
      Goto(&end);
    } else {
      DCHECK_EQ(mode, Object::Conversion::kToNumber);
      if (bigint_handling == BigIntHandling::kThrow) {
        Goto(&if_inputisother);
      } else {
        DCHECK_EQ(bigint_handling, BigIntHandling::kConvertToNumber);
        var_result.Bind(CallRuntime(Runtime::kBigIntToNumber, context, input));
        Goto(&end);
      }
    }

    BIND(&if_inputisoddball);
    {
      // The {input} is an Oddball, we just need to load the Number value of it.
      var_result.Bind(LoadObjectField(input, Oddball::kToNumberOffset));
      Goto(&end);
    }

    BIND(&if_inputisreceiver);
    {
      // The {input} is a JSReceiver, we need to convert it to a Primitive first
      // using the ToPrimitive type conversion, preferably yielding a Number.
      Callable callable = CodeFactory::NonPrimitiveToPrimitive(
          isolate(), ToPrimitiveHint::kNumber);
      TNode<Object> result = CallStub(callable, context, input);

      // Check if the {result} is already a Number/Numeric.
      Label if_done(this), if_notdone(this);
      Branch(mode == Object::Conversion::kToNumber ? IsNumber(result)
                                                   : IsNumeric(result),
             &if_done, &if_notdone);

      BIND(&if_done);
      {
        // The ToPrimitive conversion already gave us a Number/Numeric, so we're
        // done.
        var_result.Bind(result);
        Goto(&end);
      }

      BIND(&if_notdone);
      {
        // We now have a Primitive {result}, but it's not yet a Number/Numeric.
        var_input.Bind(result);
        Goto(&loop);
      }
    }

    BIND(&if_inputisother);
    {
      // The {input} is something else (e.g. Symbol), let the runtime figure
      // out the correct exception.
      // Note: We cannot tail call to the runtime here, as js-to-wasm
      // trampolines also use this code currently, and they declare all
      // outgoing parameters as untagged, while we would push a tagged
      // object here.
      auto function_id = mode == Object::Conversion::kToNumber
                             ? Runtime::kToNumber
                             : Runtime::kToNumeric;
      var_result.Bind(CallRuntime(function_id, context, input));
      Goto(&end);
    }
  }

  BIND(&end);
  if (mode == Object::Conversion::kToNumeric) {
    CSA_ASSERT(this, IsNumeric(var_result.value()));
  } else {
    DCHECK_EQ(mode, Object::Conversion::kToNumber);
    CSA_ASSERT(this, IsNumber(var_result.value()));
  }
  return var_result.value();
}

TNode<Number> CodeStubAssembler::NonNumberToNumber(
    SloppyTNode<Context> context, SloppyTNode<HeapObject> input,
    BigIntHandling bigint_handling) {
  return CAST(NonNumberToNumberOrNumeric(
      context, input, Object::Conversion::kToNumber, bigint_handling));
}

TNode<Numeric> CodeStubAssembler::NonNumberToNumeric(
    SloppyTNode<Context> context, SloppyTNode<HeapObject> input) {
  Node* result = NonNumberToNumberOrNumeric(context, input,
                                            Object::Conversion::kToNumeric);
  CSA_SLOW_ASSERT(this, IsNumeric(result));
  return UncheckedCast<Numeric>(result);
}

TNode<Number> CodeStubAssembler::ToNumber_Inline(SloppyTNode<Context> context,
                                                 SloppyTNode<Object> input) {
  TVARIABLE(Number, var_result);
  Label end(this), not_smi(this, Label::kDeferred);

  GotoIfNot(TaggedIsSmi(input), &not_smi);
  var_result = CAST(input);
  Goto(&end);

  BIND(&not_smi);
  {
    var_result = Select<Number>(
        IsHeapNumber(CAST(input)), [=] { return CAST(input); },
        [=] {
          return CAST(
              CallBuiltin(Builtins::kNonNumberToNumber, context, input));
        });
    Goto(&end);
  }

  BIND(&end);
  return var_result.value();
}

TNode<Number> CodeStubAssembler::ToNumber(SloppyTNode<Context> context,
                                          SloppyTNode<Object> input,
                                          BigIntHandling bigint_handling) {
  TVARIABLE(Number, var_result);
  Label end(this);

  Label not_smi(this, Label::kDeferred);
  GotoIfNot(TaggedIsSmi(input), &not_smi);
  TNode<Smi> input_smi = CAST(input);
  var_result = input_smi;
  Goto(&end);

  BIND(&not_smi);
  {
    Label not_heap_number(this, Label::kDeferred);
    TNode<HeapObject> input_ho = CAST(input);
    GotoIfNot(IsHeapNumber(input_ho), &not_heap_number);

    TNode<HeapNumber> input_hn = CAST(input_ho);
    var_result = input_hn;
    Goto(&end);

    BIND(&not_heap_number);
    {
      var_result = NonNumberToNumber(context, input_ho, bigint_handling);
      Goto(&end);
    }
  }

  BIND(&end);
  return var_result.value();
}

TNode<BigInt> CodeStubAssembler::ToBigInt(SloppyTNode<Context> context,
                                          SloppyTNode<Object> input) {
  TVARIABLE(BigInt, var_result);
  Label if_bigint(this), done(this), if_throw(this);

  GotoIf(TaggedIsSmi(input), &if_throw);
  GotoIf(IsBigInt(CAST(input)), &if_bigint);
  var_result = CAST(CallRuntime(Runtime::kToBigInt, context, input));
  Goto(&done);

  BIND(&if_bigint);
  var_result = CAST(input);
  Goto(&done);

  BIND(&if_throw);
  ThrowTypeError(context, MessageTemplate::kBigIntFromObject, input);

  BIND(&done);
  return var_result.value();
}

void CodeStubAssembler::TaggedToNumeric(Node* context, Node* value, Label* done,
                                        Variable* var_numeric) {
  TaggedToNumeric(context, value, done, var_numeric, nullptr);
}

void CodeStubAssembler::TaggedToNumericWithFeedback(Node* context, Node* value,
                                                    Label* done,
                                                    Variable* var_numeric,
                                                    Variable* var_feedback) {
  DCHECK_NOT_NULL(var_feedback);
  TaggedToNumeric(context, value, done, var_numeric, var_feedback);
}

void CodeStubAssembler::TaggedToNumeric(Node* context, Node* value, Label* done,
                                        Variable* var_numeric,
                                        Variable* var_feedback) {
  var_numeric->Bind(value);
  Label if_smi(this), if_heapnumber(this), if_bigint(this), if_oddball(this);
  GotoIf(TaggedIsSmi(value), &if_smi);
  TNode<Map> map = LoadMap(value);
  GotoIf(IsHeapNumberMap(map), &if_heapnumber);
  TNode<Uint16T> instance_type = LoadMapInstanceType(map);
  GotoIf(IsBigIntInstanceType(instance_type), &if_bigint);

  // {value} is not a Numeric yet.
  GotoIf(Word32Equal(instance_type, Int32Constant(ODDBALL_TYPE)), &if_oddball);
  var_numeric->Bind(CallBuiltin(Builtins::kNonNumberToNumeric, context, value));
  OverwriteFeedback(var_feedback, BinaryOperationFeedback::kAny);
  Goto(done);

  BIND(&if_smi);
  OverwriteFeedback(var_feedback, BinaryOperationFeedback::kSignedSmall);
  Goto(done);

  BIND(&if_heapnumber);
  OverwriteFeedback(var_feedback, BinaryOperationFeedback::kNumber);
  Goto(done);

  BIND(&if_bigint);
  OverwriteFeedback(var_feedback, BinaryOperationFeedback::kBigInt);
  Goto(done);

  BIND(&if_oddball);
  OverwriteFeedback(var_feedback, BinaryOperationFeedback::kNumberOrOddball);
  var_numeric->Bind(LoadObjectField(value, Oddball::kToNumberOffset));
  Goto(done);
}

// ES#sec-touint32
TNode<Number> CodeStubAssembler::ToUint32(SloppyTNode<Context> context,
                                          SloppyTNode<Object> input) {
  TNode<Float64T> const float_zero = Float64Constant(0.0);
  TNode<Float64T> const float_two_32 =
      Float64Constant(static_cast<double>(1ULL << 32));

  Label out(this);

  VARIABLE(var_result, MachineRepresentation::kTagged, input);

  // Early exit for positive smis.
  {
    // TODO(jgruber): This branch and the recheck below can be removed once we
    // have a ToNumber with multiple exits.
    Label next(this, Label::kDeferred);
    Branch(TaggedIsPositiveSmi(input), &out, &next);
    BIND(&next);
  }

  TNode<Number> const number = ToNumber(context, input);
  var_result.Bind(number);

  // Perhaps we have a positive smi now.
  {
    Label next(this, Label::kDeferred);
    Branch(TaggedIsPositiveSmi(number), &out, &next);
    BIND(&next);
  }

  Label if_isnegativesmi(this), if_isheapnumber(this);
  Branch(TaggedIsSmi(number), &if_isnegativesmi, &if_isheapnumber);

  BIND(&if_isnegativesmi);
  {
    TNode<Int32T> const uint32_value = SmiToInt32(CAST(number));
    TNode<Float64T> float64_value = ChangeUint32ToFloat64(uint32_value);
    var_result.Bind(AllocateHeapNumberWithValue(float64_value));
    Goto(&out);
  }

  BIND(&if_isheapnumber);
  {
    Label return_zero(this);
    TNode<Float64T> const value = LoadHeapNumberValue(CAST(number));

    {
      // +-0.
      Label next(this);
      Branch(Float64Equal(value, float_zero), &return_zero, &next);
      BIND(&next);
    }

    {
      // NaN.
      Label next(this);
      Branch(Float64Equal(value, value), &next, &return_zero);
      BIND(&next);
    }

    {
      // +Infinity.
      Label next(this);
      TNode<Float64T> const positive_infinity =
          Float64Constant(std::numeric_limits<double>::infinity());
      Branch(Float64Equal(value, positive_infinity), &return_zero, &next);
      BIND(&next);
    }

    {
      // -Infinity.
      Label next(this);
      TNode<Float64T> const negative_infinity =
          Float64Constant(-1.0 * std::numeric_limits<double>::infinity());
      Branch(Float64Equal(value, negative_infinity), &return_zero, &next);
      BIND(&next);
    }

    // * Let int be the mathematical value that is the same sign as number and
    //   whose magnitude is floor(abs(number)).
    // * Let int32bit be int modulo 2^32.
    // * Return int32bit.
    {
      TNode<Float64T> x = Float64Trunc(value);
      x = Float64Mod(x, float_two_32);
      x = Float64Add(x, float_two_32);
      x = Float64Mod(x, float_two_32);

      TNode<Number> const result = ChangeFloat64ToTagged(x);
      var_result.Bind(result);
      Goto(&out);
    }

    BIND(&return_zero);
    {
      var_result.Bind(SmiConstant(0));
      Goto(&out);
    }
  }

  BIND(&out);
  return CAST(var_result.value());
}

TNode<String> CodeStubAssembler::ToString_Inline(SloppyTNode<Context> context,
                                                 SloppyTNode<Object> input) {
  VARIABLE(var_result, MachineRepresentation::kTagged, input);
  Label stub_call(this, Label::kDeferred), out(this);

  GotoIf(TaggedIsSmi(input), &stub_call);
  Branch(IsString(CAST(input)), &out, &stub_call);

  BIND(&stub_call);
  var_result.Bind(CallBuiltin(Builtins::kToString, context, input));
  Goto(&out);

  BIND(&out);
  return CAST(var_result.value());
}

TNode<JSReceiver> CodeStubAssembler::ToObject(SloppyTNode<Context> context,
                                              SloppyTNode<Object> input) {
  return CAST(CallBuiltin(Builtins::kToObject, context, input));
}

TNode<JSReceiver> CodeStubAssembler::ToObject_Inline(TNode<Context> context,
                                                     TNode<Object> input) {
  TVARIABLE(JSReceiver, result);
  Label if_isreceiver(this), if_isnotreceiver(this, Label::kDeferred);
  Label done(this);

  BranchIfJSReceiver(input, &if_isreceiver, &if_isnotreceiver);

  BIND(&if_isreceiver);
  {
    result = CAST(input);
    Goto(&done);
  }

  BIND(&if_isnotreceiver);
  {
    result = ToObject(context, input);
    Goto(&done);
  }

  BIND(&done);
  return result.value();
}

TNode<Smi> CodeStubAssembler::ToSmiIndex(TNode<Context> context,
                                         TNode<Object> input,
                                         Label* range_error) {
  TVARIABLE(Smi, result);
  Label check_undefined(this), return_zero(this), defined(this),
      negative_check(this), done(this);

  GotoIfNot(TaggedIsSmi(input), &check_undefined);
  result = CAST(input);
  Goto(&negative_check);

  BIND(&check_undefined);
  Branch(IsUndefined(input), &return_zero, &defined);

  BIND(&defined);
  TNode<Number> integer_input =
      CAST(CallBuiltin(Builtins::kToInteger_TruncateMinusZero, context, input));
  GotoIfNot(TaggedIsSmi(integer_input), range_error);
  result = CAST(integer_input);
  Goto(&negative_check);

  BIND(&negative_check);
  Branch(SmiLessThan(result.value(), SmiConstant(0)), range_error, &done);

  BIND(&return_zero);
  result = SmiConstant(0);
  Goto(&done);

  BIND(&done);
  return result.value();
}

TNode<Smi> CodeStubAssembler::ToSmiLength(TNode<Context> context,
                                          TNode<Object> input,
                                          Label* range_error) {
  TVARIABLE(Smi, result);
  Label to_integer(this), negative_check(this),
      heap_number_negative_check(this), return_zero(this), done(this);

  GotoIfNot(TaggedIsSmi(input), &to_integer);
  result = CAST(input);
  Goto(&negative_check);

  BIND(&to_integer);
  {
    TNode<Number> integer_input = CAST(
        CallBuiltin(Builtins::kToInteger_TruncateMinusZero, context, input));
    GotoIfNot(TaggedIsSmi(integer_input), &heap_number_negative_check);
    result = CAST(integer_input);
    Goto(&negative_check);

    // integer_input can still be a negative HeapNumber here.
    BIND(&heap_number_negative_check);
    TNode<HeapNumber> heap_number_input = CAST(integer_input);
    Branch(IsTrue(CallBuiltin(Builtins::kLessThan, context, heap_number_input,
                              SmiConstant(0))),
           &return_zero, range_error);
  }

  BIND(&negative_check);
  Branch(SmiLessThan(result.value(), SmiConstant(0)), &return_zero, &done);

  BIND(&return_zero);
  result = SmiConstant(0);
  Goto(&done);

  BIND(&done);
  return result.value();
}

TNode<Number> CodeStubAssembler::ToLength_Inline(SloppyTNode<Context> context,
                                                 SloppyTNode<Object> input) {
  TNode<Smi> smi_zero = SmiConstant(0);
  return Select<Number>(
      TaggedIsSmi(input), [=] { return SmiMax(CAST(input), smi_zero); },
      [=] { return CAST(CallBuiltin(Builtins::kToLength, context, input)); });
}

TNode<Number> CodeStubAssembler::ToInteger_Inline(
    SloppyTNode<Context> context, SloppyTNode<Object> input,
    ToIntegerTruncationMode mode) {
  Builtins::Name builtin = (mode == kNoTruncation)
                               ? Builtins::kToInteger
                               : Builtins::kToInteger_TruncateMinusZero;
  return Select<Number>(
      TaggedIsSmi(input), [=] { return CAST(input); },
      [=] { return CAST(CallBuiltin(builtin, context, input)); });
}

TNode<Number> CodeStubAssembler::ToInteger(SloppyTNode<Context> context,
                                           SloppyTNode<Object> input,
                                           ToIntegerTruncationMode mode) {
  // We might need to loop once for ToNumber conversion.
  TVARIABLE(Object, var_arg, input);
  Label loop(this, &var_arg), out(this);
  Goto(&loop);
  BIND(&loop);
  {
    // Shared entry points.
    Label return_zero(this, Label::kDeferred);

    // Load the current {arg} value.
    TNode<Object> arg = var_arg.value();

    // Check if {arg} is a Smi.
    GotoIf(TaggedIsSmi(arg), &out);

    // Check if {arg} is a HeapNumber.
    Label if_argisheapnumber(this),
        if_argisnotheapnumber(this, Label::kDeferred);
    Branch(IsHeapNumber(CAST(arg)), &if_argisheapnumber,
           &if_argisnotheapnumber);

    BIND(&if_argisheapnumber);
    {
      TNode<HeapNumber> arg_hn = CAST(arg);
      // Load the floating-point value of {arg}.
      TNode<Float64T> arg_value = LoadHeapNumberValue(arg_hn);

      // Check if {arg} is NaN.
      GotoIfNot(Float64Equal(arg_value, arg_value), &return_zero);

      // Truncate {arg} towards zero.
      TNode<Float64T> value = Float64Trunc(arg_value);

      if (mode == kTruncateMinusZero) {
        // Truncate -0.0 to 0.
        GotoIf(Float64Equal(value, Float64Constant(0.0)), &return_zero);
      }

      var_arg = ChangeFloat64ToTagged(value);
      Goto(&out);
    }

    BIND(&if_argisnotheapnumber);
    {
      // Need to convert {arg} to a Number first.
      var_arg = UncheckedCast<Object>(
          CallBuiltin(Builtins::kNonNumberToNumber, context, arg));
      Goto(&loop);
    }

    BIND(&return_zero);
    var_arg = SmiConstant(0);
    Goto(&out);
  }

  BIND(&out);
  if (mode == kTruncateMinusZero) {
    CSA_ASSERT(this, IsNumberNormalized(CAST(var_arg.value())));
  }
  return CAST(var_arg.value());
}

TNode<Uint32T> CodeStubAssembler::DecodeWord32(SloppyTNode<Word32T> word32,
                                               uint32_t shift, uint32_t mask) {
  return UncheckedCast<Uint32T>(Word32Shr(
      Word32And(word32, Int32Constant(mask)), static_cast<int>(shift)));
}

TNode<UintPtrT> CodeStubAssembler::DecodeWord(SloppyTNode<WordT> word,
                                              uint32_t shift, uint32_t mask) {
  return Unsigned(
      WordShr(WordAnd(word, IntPtrConstant(mask)), static_cast<int>(shift)));
}

TNode<WordT> CodeStubAssembler::UpdateWord(TNode<WordT> word,
                                           TNode<WordT> value, uint32_t shift,
                                           uint32_t mask) {
  TNode<WordT> encoded_value = WordShl(value, static_cast<int>(shift));
  TNode<IntPtrT> inverted_mask = IntPtrConstant(~static_cast<intptr_t>(mask));
  // Ensure the {value} fits fully in the mask.
  CSA_ASSERT(this, WordEqual(WordAnd(encoded_value, inverted_mask),
                             IntPtrConstant(0)));
  return WordOr(WordAnd(word, inverted_mask), encoded_value);
}

void CodeStubAssembler::SetCounter(StatsCounter* counter, int value) {
  if (FLAG_native_code_counters && counter->Enabled()) {
    TNode<ExternalReference> counter_address =
        ExternalConstant(ExternalReference::Create(counter));
    StoreNoWriteBarrier(MachineRepresentation::kWord32, counter_address,
                        Int32Constant(value));
  }
}

void CodeStubAssembler::IncrementCounter(StatsCounter* counter, int delta) {
  DCHECK_GT(delta, 0);
  if (FLAG_native_code_counters && counter->Enabled()) {
    TNode<ExternalReference> counter_address =
        ExternalConstant(ExternalReference::Create(counter));
    // This operation has to be exactly 32-bit wide in case the external
    // reference table redirects the counter to a uint32_t dummy_stats_counter_
    // field.
    Node* value = Load(MachineType::Int32(), counter_address);
    value = Int32Add(value, Int32Constant(delta));
    StoreNoWriteBarrier(MachineRepresentation::kWord32, counter_address, value);
  }
}

void CodeStubAssembler::DecrementCounter(StatsCounter* counter, int delta) {
  DCHECK_GT(delta, 0);
  if (FLAG_native_code_counters && counter->Enabled()) {
    TNode<ExternalReference> counter_address =
        ExternalConstant(ExternalReference::Create(counter));
    // This operation has to be exactly 32-bit wide in case the external
    // reference table redirects the counter to a uint32_t dummy_stats_counter_
    // field.
    Node* value = Load(MachineType::Int32(), counter_address);
    value = Int32Sub(value, Int32Constant(delta));
    StoreNoWriteBarrier(MachineRepresentation::kWord32, counter_address, value);
  }
}

void CodeStubAssembler::Increment(Variable* variable, int value,
                                  ParameterMode mode) {
  DCHECK_IMPLIES(mode == INTPTR_PARAMETERS,
                 variable->rep() == MachineType::PointerRepresentation());
  DCHECK_IMPLIES(mode == SMI_PARAMETERS, CanBeTaggedSigned(variable->rep()));
  variable->Bind(IntPtrOrSmiAdd(variable->value(),
                                IntPtrOrSmiConstant(value, mode), mode));
}

void CodeStubAssembler::Use(Label* label) {
  GotoIf(Word32Equal(Int32Constant(0), Int32Constant(1)), label);
}

void CodeStubAssembler::TryToName(Node* key, Label* if_keyisindex,
                                  Variable* var_index, Label* if_keyisunique,
                                  Variable* var_unique, Label* if_bailout,
                                  Label* if_notinternalized) {
  DCHECK_EQ(MachineType::PointerRepresentation(), var_index->rep());
  DCHECK_EQ(MachineRepresentation::kTagged, var_unique->rep());
  Comment("TryToName");

  Label if_hascachedindex(this), if_keyisnotindex(this), if_thinstring(this),
      if_keyisother(this, Label::kDeferred);
  // Handle Smi and HeapNumber keys.
  var_index->Bind(TryToIntptr(key, &if_keyisnotindex));
  Goto(if_keyisindex);

  BIND(&if_keyisnotindex);
  TNode<Map> key_map = LoadMap(key);
  var_unique->Bind(key);
  // Symbols are unique.
  GotoIf(IsSymbolMap(key_map), if_keyisunique);
  TNode<Uint16T> key_instance_type = LoadMapInstanceType(key_map);
  // Miss if |key| is not a String.
  STATIC_ASSERT(FIRST_NAME_TYPE == FIRST_TYPE);
  GotoIfNot(IsStringInstanceType(key_instance_type), &if_keyisother);

  // |key| is a String. Check if it has a cached array index.
  TNode<Uint32T> hash = LoadNameHashField(key);
  GotoIf(IsClearWord32(hash, Name::kDoesNotContainCachedArrayIndexMask),
         &if_hascachedindex);
  // No cached array index. If the string knows that it contains an index,
  // then it must be an uncacheable index. Handle this case in the runtime.
  GotoIf(IsClearWord32(hash, Name::kIsNotArrayIndexMask), if_bailout);
  // Check if we have a ThinString.
  GotoIf(InstanceTypeEqual(key_instance_type, THIN_STRING_TYPE),
         &if_thinstring);
  GotoIf(InstanceTypeEqual(key_instance_type, THIN_ONE_BYTE_STRING_TYPE),
         &if_thinstring);
  // Finally, check if |key| is internalized.
  STATIC_ASSERT(kNotInternalizedTag != 0);
  GotoIf(IsSetWord32(key_instance_type, kIsNotInternalizedMask),
         if_notinternalized != nullptr ? if_notinternalized : if_bailout);
  Goto(if_keyisunique);

  BIND(&if_thinstring);
  var_unique->Bind(
      LoadObjectField<String>(CAST(key), ThinString::kActualOffset));
  Goto(if_keyisunique);

  BIND(&if_hascachedindex);
  var_index->Bind(DecodeWordFromWord32<Name::ArrayIndexValueBits>(hash));
  Goto(if_keyisindex);

  BIND(&if_keyisother);
  GotoIfNot(InstanceTypeEqual(key_instance_type, ODDBALL_TYPE), if_bailout);
  var_unique->Bind(LoadObjectField(key, Oddball::kToStringOffset));
  Goto(if_keyisunique);
}

void CodeStubAssembler::TryInternalizeString(
    Node* string, Label* if_index, Variable* var_index, Label* if_internalized,
    Variable* var_internalized, Label* if_not_internalized, Label* if_bailout) {
  DCHECK(var_index->rep() == MachineType::PointerRepresentation());
  DCHECK_EQ(var_internalized->rep(), MachineRepresentation::kTagged);
  CSA_SLOW_ASSERT(this, IsString(string));
  TNode<ExternalReference> function =
      ExternalConstant(ExternalReference::try_internalize_string_function());
  TNode<ExternalReference> const isolate_ptr =
      ExternalConstant(ExternalReference::isolate_address(isolate()));
  Node* result =
      CallCFunction(function, MachineType::AnyTagged(),
                    std::make_pair(MachineType::Pointer(), isolate_ptr),
                    std::make_pair(MachineType::AnyTagged(), string));
  Label internalized(this);
  GotoIf(TaggedIsNotSmi(result), &internalized);
  TNode<IntPtrT> word_result = SmiUntag(result);
  GotoIf(IntPtrEqual(word_result, IntPtrConstant(ResultSentinel::kNotFound)),
         if_not_internalized);
  GotoIf(IntPtrEqual(word_result, IntPtrConstant(ResultSentinel::kUnsupported)),
         if_bailout);
  var_index->Bind(word_result);
  Goto(if_index);

  BIND(&internalized);
  var_internalized->Bind(result);
  Goto(if_internalized);
}

template <typename Dictionary>
TNode<IntPtrT> CodeStubAssembler::EntryToIndex(TNode<IntPtrT> entry,
                                               int field_index) {
  TNode<IntPtrT> entry_index =
      IntPtrMul(entry, IntPtrConstant(Dictionary::kEntrySize));
  return IntPtrAdd(entry_index, IntPtrConstant(Dictionary::kElementsStartIndex +
                                               field_index));
}

template <typename T>
TNode<T> CodeStubAssembler::LoadDescriptorArrayElement(
    TNode<DescriptorArray> object, TNode<IntPtrT> index,
    int additional_offset) {
  return LoadArrayElement<DescriptorArray, T>(
      object, DescriptorArray::kHeaderSize, index, additional_offset);
}

TNode<Name> CodeStubAssembler::LoadKeyByKeyIndex(
    TNode<DescriptorArray> container, TNode<IntPtrT> key_index) {
  return CAST(LoadDescriptorArrayElement<HeapObject>(container, key_index, 0));
}

TNode<Uint32T> CodeStubAssembler::LoadDetailsByKeyIndex(
    TNode<DescriptorArray> container, TNode<IntPtrT> key_index) {
  const int kKeyToDetailsOffset =
      DescriptorArray::kEntryDetailsOffset - DescriptorArray::kEntryKeyOffset;
  return Unsigned(LoadAndUntagToWord32ArrayElement(
      container, DescriptorArray::kHeaderSize, key_index, kKeyToDetailsOffset));
}

TNode<Object> CodeStubAssembler::LoadValueByKeyIndex(
    TNode<DescriptorArray> container, TNode<IntPtrT> key_index) {
  const int kKeyToValueOffset =
      DescriptorArray::kEntryValueOffset - DescriptorArray::kEntryKeyOffset;
  return LoadDescriptorArrayElement<Object>(container, key_index,
                                            kKeyToValueOffset);
}

TNode<MaybeObject> CodeStubAssembler::LoadFieldTypeByKeyIndex(
    TNode<DescriptorArray> container, TNode<IntPtrT> key_index) {
  const int kKeyToValueOffset =
      DescriptorArray::kEntryValueOffset - DescriptorArray::kEntryKeyOffset;
  return LoadDescriptorArrayElement<MaybeObject>(container, key_index,
                                                 kKeyToValueOffset);
}

TNode<IntPtrT> CodeStubAssembler::DescriptorEntryToIndex(
    TNode<IntPtrT> descriptor_entry) {
  return IntPtrMul(descriptor_entry,
                   IntPtrConstant(DescriptorArray::kEntrySize));
}

TNode<Name> CodeStubAssembler::LoadKeyByDescriptorEntry(
    TNode<DescriptorArray> container, TNode<IntPtrT> descriptor_entry) {
  return CAST(LoadDescriptorArrayElement<HeapObject>(
      container, DescriptorEntryToIndex(descriptor_entry),
      DescriptorArray::ToKeyIndex(0) * kTaggedSize));
}

TNode<Name> CodeStubAssembler::LoadKeyByDescriptorEntry(
    TNode<DescriptorArray> container, int descriptor_entry) {
  return CAST(LoadDescriptorArrayElement<HeapObject>(
      container, IntPtrConstant(0),
      DescriptorArray::ToKeyIndex(descriptor_entry) * kTaggedSize));
}

TNode<Uint32T> CodeStubAssembler::LoadDetailsByDescriptorEntry(
    TNode<DescriptorArray> container, TNode<IntPtrT> descriptor_entry) {
  return Unsigned(LoadAndUntagToWord32ArrayElement(
      container, DescriptorArray::kHeaderSize,
      DescriptorEntryToIndex(descriptor_entry),
      DescriptorArray::ToDetailsIndex(0) * kTaggedSize));
}

TNode<Uint32T> CodeStubAssembler::LoadDetailsByDescriptorEntry(
    TNode<DescriptorArray> container, int descriptor_entry) {
  return Unsigned(LoadAndUntagToWord32ArrayElement(
      container, DescriptorArray::kHeaderSize, IntPtrConstant(0),
      DescriptorArray::ToDetailsIndex(descriptor_entry) * kTaggedSize));
}

TNode<Object> CodeStubAssembler::LoadValueByDescriptorEntry(
    TNode<DescriptorArray> container, int descriptor_entry) {
  return LoadDescriptorArrayElement<Object>(
      container, IntPtrConstant(0),
      DescriptorArray::ToValueIndex(descriptor_entry) * kTaggedSize);
}

TNode<MaybeObject> CodeStubAssembler::LoadFieldTypeByDescriptorEntry(
    TNode<DescriptorArray> container, TNode<IntPtrT> descriptor_entry) {
  return LoadDescriptorArrayElement<MaybeObject>(
      container, DescriptorEntryToIndex(descriptor_entry),
      DescriptorArray::ToValueIndex(0) * kTaggedSize);
}

template TNode<IntPtrT> CodeStubAssembler::EntryToIndex<NameDictionary>(
    TNode<IntPtrT>, int);
template TNode<IntPtrT> CodeStubAssembler::EntryToIndex<GlobalDictionary>(
    TNode<IntPtrT>, int);
template TNode<IntPtrT> CodeStubAssembler::EntryToIndex<NumberDictionary>(
    TNode<IntPtrT>, int);

// This must be kept in sync with HashTableBase::ComputeCapacity().
TNode<IntPtrT> CodeStubAssembler::HashTableComputeCapacity(
    TNode<IntPtrT> at_least_space_for) {
  TNode<IntPtrT> capacity = IntPtrRoundUpToPowerOfTwo32(
      IntPtrAdd(at_least_space_for, WordShr(at_least_space_for, 1)));
  return IntPtrMax(capacity, IntPtrConstant(HashTableBase::kMinCapacity));
}

TNode<IntPtrT> CodeStubAssembler::IntPtrMax(SloppyTNode<IntPtrT> left,
                                            SloppyTNode<IntPtrT> right) {
  intptr_t left_constant;
  intptr_t right_constant;
  if (ToIntPtrConstant(left, &left_constant) &&
      ToIntPtrConstant(right, &right_constant)) {
    return IntPtrConstant(std::max(left_constant, right_constant));
  }
  return SelectConstant<IntPtrT>(IntPtrGreaterThanOrEqual(left, right), left,
                                 right);
}

TNode<IntPtrT> CodeStubAssembler::IntPtrMin(SloppyTNode<IntPtrT> left,
                                            SloppyTNode<IntPtrT> right) {
  intptr_t left_constant;
  intptr_t right_constant;
  if (ToIntPtrConstant(left, &left_constant) &&
      ToIntPtrConstant(right, &right_constant)) {
    return IntPtrConstant(std::min(left_constant, right_constant));
  }
  return SelectConstant<IntPtrT>(IntPtrLessThanOrEqual(left, right), left,
                                 right);
}

template <>
TNode<HeapObject> CodeStubAssembler::LoadName<NameDictionary>(
    TNode<HeapObject> key) {
  CSA_ASSERT(this, Word32Or(IsTheHole(key), IsName(key)));
  return key;
}

template <>
TNode<HeapObject> CodeStubAssembler::LoadName<GlobalDictionary>(
    TNode<HeapObject> key) {
  TNode<PropertyCell> property_cell = CAST(key);
  return CAST(LoadObjectField(property_cell, PropertyCell::kNameOffset));
}

template <typename Dictionary>
void CodeStubAssembler::NameDictionaryLookup(
    TNode<Dictionary> dictionary, TNode<Name> unique_name, Label* if_found,
    TVariable<IntPtrT>* var_name_index, Label* if_not_found, LookupMode mode) {
  static_assert(std::is_same<Dictionary, NameDictionary>::value ||
                    std::is_same<Dictionary, GlobalDictionary>::value,
                "Unexpected NameDictionary");
  DCHECK_EQ(MachineType::PointerRepresentation(), var_name_index->rep());
  DCHECK_IMPLIES(mode == kFindInsertionIndex, if_found == nullptr);
  Comment("NameDictionaryLookup");
  CSA_ASSERT(this, IsUniqueName(unique_name));

  TNode<IntPtrT> capacity = SmiUntag(GetCapacity<Dictionary>(dictionary));
  TNode<IntPtrT> mask = IntPtrSub(capacity, IntPtrConstant(1));
  TNode<UintPtrT> hash = ChangeUint32ToWord(LoadNameHash(unique_name));

  // See Dictionary::FirstProbe().
  TNode<IntPtrT> count = IntPtrConstant(0);
  TNode<IntPtrT> entry = Signed(WordAnd(hash, mask));
  TNode<Oddball> undefined = UndefinedConstant();

  // Appease the variable merging algorithm for "Goto(&loop)" below.
  *var_name_index = IntPtrConstant(0);

  TVARIABLE(IntPtrT, var_count, count);
  TVARIABLE(IntPtrT, var_entry, entry);
  Variable* loop_vars[] = {&var_count, &var_entry, var_name_index};
  Label loop(this, arraysize(loop_vars), loop_vars);
  Goto(&loop);
  BIND(&loop);
  {
    TNode<IntPtrT> entry = var_entry.value();

    TNode<IntPtrT> index = EntryToIndex<Dictionary>(entry);
    *var_name_index = index;

    TNode<HeapObject> current =
        CAST(UnsafeLoadFixedArrayElement(dictionary, index));
    GotoIf(TaggedEqual(current, undefined), if_not_found);
    if (mode == kFindExisting) {
      current = LoadName<Dictionary>(current);
      GotoIf(TaggedEqual(current, unique_name), if_found);
    } else {
      DCHECK_EQ(kFindInsertionIndex, mode);
      GotoIf(TaggedEqual(current, TheHoleConstant()), if_not_found);
    }

    // See Dictionary::NextProbe().
    Increment(&var_count);
    entry = Signed(WordAnd(IntPtrAdd(entry, var_count.value()), mask));

    var_entry = entry;
    Goto(&loop);
  }
}

// Instantiate template methods to workaround GCC compilation issue.
template V8_EXPORT_PRIVATE void
CodeStubAssembler::NameDictionaryLookup<NameDictionary>(TNode<NameDictionary>,
                                                        TNode<Name>, Label*,
                                                        TVariable<IntPtrT>*,
                                                        Label*, LookupMode);
template V8_EXPORT_PRIVATE void CodeStubAssembler::NameDictionaryLookup<
    GlobalDictionary>(TNode<GlobalDictionary>, TNode<Name>, Label*,
                      TVariable<IntPtrT>*, Label*, LookupMode);

Node* CodeStubAssembler::ComputeUnseededHash(Node* key) {
  // See v8::internal::ComputeUnseededHash()
  TNode<Word32T> hash = TruncateIntPtrToInt32(key);
  hash = Int32Add(Word32Xor(hash, Int32Constant(0xFFFFFFFF)),
                  Word32Shl(hash, Int32Constant(15)));
  hash = Word32Xor(hash, Word32Shr(hash, Int32Constant(12)));
  hash = Int32Add(hash, Word32Shl(hash, Int32Constant(2)));
  hash = Word32Xor(hash, Word32Shr(hash, Int32Constant(4)));
  hash = Int32Mul(hash, Int32Constant(2057));
  hash = Word32Xor(hash, Word32Shr(hash, Int32Constant(16)));
  return Word32And(hash, Int32Constant(0x3FFFFFFF));
}

Node* CodeStubAssembler::ComputeSeededHash(Node* key) {
  TNode<ExternalReference> const function_addr =
      ExternalConstant(ExternalReference::compute_integer_hash());
  TNode<ExternalReference> const isolate_ptr =
      ExternalConstant(ExternalReference::isolate_address(isolate()));

  MachineType type_ptr = MachineType::Pointer();
  MachineType type_uint32 = MachineType::Uint32();

  Node* const result = CallCFunction(
      function_addr, type_uint32, std::make_pair(type_ptr, isolate_ptr),
      std::make_pair(type_uint32, TruncateIntPtrToInt32(key)));
  return result;
}

void CodeStubAssembler::NumberDictionaryLookup(
    TNode<NumberDictionary> dictionary, TNode<IntPtrT> intptr_index,
    Label* if_found, TVariable<IntPtrT>* var_entry, Label* if_not_found) {
  CSA_ASSERT(this, IsNumberDictionary(dictionary));
  DCHECK_EQ(MachineType::PointerRepresentation(), var_entry->rep());
  Comment("NumberDictionaryLookup");

  TNode<IntPtrT> capacity = SmiUntag(GetCapacity<NumberDictionary>(dictionary));
  TNode<IntPtrT> mask = IntPtrSub(capacity, IntPtrConstant(1));

  TNode<UintPtrT> hash = ChangeUint32ToWord(ComputeSeededHash(intptr_index));
  Node* key_as_float64 = RoundIntPtrToFloat64(intptr_index);

  // See Dictionary::FirstProbe().
  TNode<IntPtrT> count = IntPtrConstant(0);
  TNode<IntPtrT> entry = Signed(WordAnd(hash, mask));

  TNode<Oddball> undefined = UndefinedConstant();
  TNode<Oddball> the_hole = TheHoleConstant();

  TVARIABLE(IntPtrT, var_count, count);
  Variable* loop_vars[] = {&var_count, var_entry};
  Label loop(this, 2, loop_vars);
  *var_entry = entry;
  Goto(&loop);
  BIND(&loop);
  {
    TNode<IntPtrT> entry = var_entry->value();

    TNode<IntPtrT> index = EntryToIndex<NumberDictionary>(entry);
    TNode<Object> current = UnsafeLoadFixedArrayElement(dictionary, index);
    GotoIf(TaggedEqual(current, undefined), if_not_found);
    Label next_probe(this);
    {
      Label if_currentissmi(this), if_currentisnotsmi(this);
      Branch(TaggedIsSmi(current), &if_currentissmi, &if_currentisnotsmi);
      BIND(&if_currentissmi);
      {
        TNode<IntPtrT> current_value = SmiUntag(CAST(current));
        Branch(WordEqual(current_value, intptr_index), if_found, &next_probe);
      }
      BIND(&if_currentisnotsmi);
      {
        GotoIf(TaggedEqual(current, the_hole), &next_probe);
        // Current must be the Number.
        TNode<Float64T> current_value = LoadHeapNumberValue(CAST(current));
        Branch(Float64Equal(current_value, key_as_float64), if_found,
               &next_probe);
      }
    }

    BIND(&next_probe);
    // See Dictionary::NextProbe().
    Increment(&var_count);
    entry = Signed(WordAnd(IntPtrAdd(entry, var_count.value()), mask));

    *var_entry = entry;
    Goto(&loop);
  }
}

TNode<Object> CodeStubAssembler::BasicLoadNumberDictionaryElement(
    TNode<NumberDictionary> dictionary, TNode<IntPtrT> intptr_index,
    Label* not_data, Label* if_hole) {
  TVARIABLE(IntPtrT, var_entry);
  Label if_found(this);
  NumberDictionaryLookup(dictionary, intptr_index, &if_found, &var_entry,
                         if_hole);
  BIND(&if_found);

  // Check that the value is a data property.
  TNode<IntPtrT> index = EntryToIndex<NumberDictionary>(var_entry.value());
  TNode<Uint32T> details =
      LoadDetailsByKeyIndex<NumberDictionary>(dictionary, index);
  TNode<Uint32T> kind = DecodeWord32<PropertyDetails::KindField>(details);
  // TODO(jkummerow): Support accessors without missing?
  GotoIfNot(Word32Equal(kind, Int32Constant(kData)), not_data);
  // Finally, load the value.
  return LoadValueByKeyIndex<NumberDictionary>(dictionary, index);
}

void CodeStubAssembler::BasicStoreNumberDictionaryElement(
    TNode<NumberDictionary> dictionary, TNode<IntPtrT> intptr_index,
    TNode<Object> value, Label* not_data, Label* if_hole, Label* read_only) {
  TVARIABLE(IntPtrT, var_entry);
  Label if_found(this);
  NumberDictionaryLookup(dictionary, intptr_index, &if_found, &var_entry,
                         if_hole);
  BIND(&if_found);

  // Check that the value is a data property.
  TNode<IntPtrT> index = EntryToIndex<NumberDictionary>(var_entry.value());
  TNode<Uint32T> details =
      LoadDetailsByKeyIndex<NumberDictionary>(dictionary, index);
  TNode<Uint32T> kind = DecodeWord32<PropertyDetails::KindField>(details);
  // TODO(jkummerow): Support accessors without missing?
  GotoIfNot(Word32Equal(kind, Int32Constant(kData)), not_data);

  // Check that the property is writeable.
  GotoIf(IsSetWord32(details, PropertyDetails::kAttributesReadOnlyMask),
         read_only);

  // Finally, store the value.
  StoreValueByKeyIndex<NumberDictionary>(dictionary, index, value);
}

template <class Dictionary>
void CodeStubAssembler::FindInsertionEntry(TNode<Dictionary> dictionary,
                                           TNode<Name> key,
                                           TVariable<IntPtrT>* var_key_index) {
  UNREACHABLE();
}

template <>
void CodeStubAssembler::FindInsertionEntry<NameDictionary>(
    TNode<NameDictionary> dictionary, TNode<Name> key,
    TVariable<IntPtrT>* var_key_index) {
  Label done(this);
  NameDictionaryLookup<NameDictionary>(dictionary, key, nullptr, var_key_index,
                                       &done, kFindInsertionIndex);
  BIND(&done);
}

template <class Dictionary>
void CodeStubAssembler::InsertEntry(TNode<Dictionary> dictionary,
                                    TNode<Name> key, TNode<Object> value,
                                    TNode<IntPtrT> index,
                                    TNode<Smi> enum_index) {
  UNREACHABLE();  // Use specializations instead.
}

template <>
void CodeStubAssembler::InsertEntry<NameDictionary>(
    TNode<NameDictionary> dictionary, TNode<Name> name, TNode<Object> value,
    TNode<IntPtrT> index, TNode<Smi> enum_index) {
  // Store name and value.
  StoreFixedArrayElement(dictionary, index, name);
  StoreValueByKeyIndex<NameDictionary>(dictionary, index, value);

  // Prepare details of the new property.
  PropertyDetails d(kData, NONE, PropertyCellType::kNoCell);
  enum_index =
      SmiShl(enum_index, PropertyDetails::DictionaryStorageField::kShift);
  // We OR over the actual index below, so we expect the initial value to be 0.
  DCHECK_EQ(0, d.dictionary_index());
  TVARIABLE(Smi, var_details, SmiOr(SmiConstant(d.AsSmi()), enum_index));

  // Private names must be marked non-enumerable.
  Label not_private(this, &var_details);
  GotoIfNot(IsPrivateSymbol(name), &not_private);
  TNode<Smi> dont_enum =
      SmiShl(SmiConstant(DONT_ENUM), PropertyDetails::AttributesField::kShift);
  var_details = SmiOr(var_details.value(), dont_enum);
  Goto(&not_private);
  BIND(&not_private);

  // Finally, store the details.
  StoreDetailsByKeyIndex<NameDictionary>(dictionary, index,
                                         var_details.value());
}

template <>
void CodeStubAssembler::InsertEntry<GlobalDictionary>(
    TNode<GlobalDictionary> dictionary, TNode<Name> key, TNode<Object> value,
    TNode<IntPtrT> index, TNode<Smi> enum_index) {
  UNIMPLEMENTED();
}

template <class Dictionary>
void CodeStubAssembler::Add(TNode<Dictionary> dictionary, TNode<Name> key,
                            TNode<Object> value, Label* bailout) {
  CSA_ASSERT(this, Word32BinaryNot(IsEmptyPropertyDictionary(dictionary)));
  TNode<Smi> capacity = GetCapacity<Dictionary>(dictionary);
  TNode<Smi> nof = GetNumberOfElements<Dictionary>(dictionary);
  TNode<Smi> new_nof = SmiAdd(nof, SmiConstant(1));
  // Require 33% to still be free after adding additional_elements.
  // Computing "x + (x >> 1)" on a Smi x does not return a valid Smi!
  // But that's OK here because it's only used for a comparison.
  TNode<Smi> required_capacity_pseudo_smi = SmiAdd(new_nof, SmiShr(new_nof, 1));
  GotoIf(SmiBelow(capacity, required_capacity_pseudo_smi), bailout);
  // Require rehashing if more than 50% of free elements are deleted elements.
  TNode<Smi> deleted = GetNumberOfDeletedElements<Dictionary>(dictionary);
  CSA_ASSERT(this, SmiAbove(capacity, new_nof));
  TNode<Smi> half_of_free_elements = SmiShr(SmiSub(capacity, new_nof), 1);
  GotoIf(SmiAbove(deleted, half_of_free_elements), bailout);

  TNode<Smi> enum_index = GetNextEnumerationIndex<Dictionary>(dictionary);
  TNode<Smi> new_enum_index = SmiAdd(enum_index, SmiConstant(1));
  TNode<Smi> max_enum_index =
      SmiConstant(PropertyDetails::DictionaryStorageField::kMax);
  GotoIf(SmiAbove(new_enum_index, max_enum_index), bailout);

  // No more bailouts after this point.
  // Operations from here on can have side effects.

  SetNextEnumerationIndex<Dictionary>(dictionary, new_enum_index);
  SetNumberOfElements<Dictionary>(dictionary, new_nof);

  TVARIABLE(IntPtrT, var_key_index);
  FindInsertionEntry<Dictionary>(dictionary, key, &var_key_index);
  InsertEntry<Dictionary>(dictionary, key, value, var_key_index.value(),
                          enum_index);
}

template void CodeStubAssembler::Add<NameDictionary>(TNode<NameDictionary>,
                                                     TNode<Name>, TNode<Object>,
                                                     Label*);

template <typename Array>
void CodeStubAssembler::LookupLinear(TNode<Name> unique_name,
                                     TNode<Array> array,
                                     TNode<Uint32T> number_of_valid_entries,
                                     Label* if_found,
                                     TVariable<IntPtrT>* var_name_index,
                                     Label* if_not_found) {
  static_assert(std::is_base_of<FixedArray, Array>::value ||
                    std::is_base_of<WeakFixedArray, Array>::value ||
                    std::is_base_of<DescriptorArray, Array>::value,
                "T must be a descendant of FixedArray or a WeakFixedArray");
  Comment("LookupLinear");
  CSA_ASSERT(this, IsUniqueName(unique_name));
  TNode<IntPtrT> first_inclusive = IntPtrConstant(Array::ToKeyIndex(0));
  TNode<IntPtrT> factor = IntPtrConstant(Array::kEntrySize);
  TNode<IntPtrT> last_exclusive = IntPtrAdd(
      first_inclusive,
      IntPtrMul(ChangeInt32ToIntPtr(number_of_valid_entries), factor));

  BuildFastLoop(
      last_exclusive, first_inclusive,
      [=](SloppyTNode<IntPtrT> name_index) {
        TNode<MaybeObject> element =
            LoadArrayElement(array, Array::kHeaderSize, name_index);
        TNode<Name> candidate_name = CAST(element);
        *var_name_index = name_index;
        GotoIf(TaggedEqual(candidate_name, unique_name), if_found);
      },
      -Array::kEntrySize, INTPTR_PARAMETERS, IndexAdvanceMode::kPre);
  Goto(if_not_found);
}

template <>
TNode<Uint32T> CodeStubAssembler::NumberOfEntries<DescriptorArray>(
    TNode<DescriptorArray> descriptors) {
  return Unsigned(LoadNumberOfDescriptors(descriptors));
}

template <>
TNode<Uint32T> CodeStubAssembler::NumberOfEntries<TransitionArray>(
    TNode<TransitionArray> transitions) {
  TNode<IntPtrT> length = LoadAndUntagWeakFixedArrayLength(transitions);
  return Select<Uint32T>(
      UintPtrLessThan(length, IntPtrConstant(TransitionArray::kFirstIndex)),
      [=] { return Unsigned(Int32Constant(0)); },
      [=] {
        return Unsigned(LoadAndUntagToWord32ArrayElement(
            transitions, WeakFixedArray::kHeaderSize,
            IntPtrConstant(TransitionArray::kTransitionLengthIndex)));
      });
}

template <typename Array>
TNode<IntPtrT> CodeStubAssembler::EntryIndexToIndex(
    TNode<Uint32T> entry_index) {
  TNode<Int32T> entry_size = Int32Constant(Array::kEntrySize);
  TNode<Word32T> index = Int32Mul(entry_index, entry_size);
  return ChangeInt32ToIntPtr(index);
}

template <typename Array>
TNode<IntPtrT> CodeStubAssembler::ToKeyIndex(TNode<Uint32T> entry_index) {
  return IntPtrAdd(IntPtrConstant(Array::ToKeyIndex(0)),
                   EntryIndexToIndex<Array>(entry_index));
}

template TNode<IntPtrT> CodeStubAssembler::ToKeyIndex<DescriptorArray>(
    TNode<Uint32T>);
template TNode<IntPtrT> CodeStubAssembler::ToKeyIndex<TransitionArray>(
    TNode<Uint32T>);

template <>
TNode<Uint32T> CodeStubAssembler::GetSortedKeyIndex<DescriptorArray>(
    TNode<DescriptorArray> descriptors, TNode<Uint32T> descriptor_number) {
  TNode<Uint32T> details =
      DescriptorArrayGetDetails(descriptors, descriptor_number);
  return DecodeWord32<PropertyDetails::DescriptorPointer>(details);
}

template <>
TNode<Uint32T> CodeStubAssembler::GetSortedKeyIndex<TransitionArray>(
    TNode<TransitionArray> transitions, TNode<Uint32T> transition_number) {
  return transition_number;
}

template <typename Array>
TNode<Name> CodeStubAssembler::GetKey(TNode<Array> array,
                                      TNode<Uint32T> entry_index) {
  static_assert(std::is_base_of<TransitionArray, Array>::value ||
                    std::is_base_of<DescriptorArray, Array>::value,
                "T must be a descendant of DescriptorArray or TransitionArray");
  const int key_offset = Array::ToKeyIndex(0) * kTaggedSize;
  TNode<MaybeObject> element =
      LoadArrayElement(array, Array::kHeaderSize,
                       EntryIndexToIndex<Array>(entry_index), key_offset);
  return CAST(element);
}

template TNode<Name> CodeStubAssembler::GetKey<DescriptorArray>(
    TNode<DescriptorArray>, TNode<Uint32T>);
template TNode<Name> CodeStubAssembler::GetKey<TransitionArray>(
    TNode<TransitionArray>, TNode<Uint32T>);

TNode<Uint32T> CodeStubAssembler::DescriptorArrayGetDetails(
    TNode<DescriptorArray> descriptors, TNode<Uint32T> descriptor_number) {
  const int details_offset = DescriptorArray::ToDetailsIndex(0) * kTaggedSize;
  return Unsigned(LoadAndUntagToWord32ArrayElement(
      descriptors, DescriptorArray::kHeaderSize,
      EntryIndexToIndex<DescriptorArray>(descriptor_number), details_offset));
}

template <typename Array>
void CodeStubAssembler::LookupBinary(TNode<Name> unique_name,
                                     TNode<Array> array,
                                     TNode<Uint32T> number_of_valid_entries,
                                     Label* if_found,
                                     TVariable<IntPtrT>* var_name_index,
                                     Label* if_not_found) {
  Comment("LookupBinary");
  TVARIABLE(Uint32T, var_low, Unsigned(Int32Constant(0)));
  TNode<Uint32T> limit =
      Unsigned(Int32Sub(NumberOfEntries<Array>(array), Int32Constant(1)));
  TVARIABLE(Uint32T, var_high, limit);
  TNode<Uint32T> hash = LoadNameHashField(unique_name);
  CSA_ASSERT(this, Word32NotEqual(hash, Int32Constant(0)));

  // Assume non-empty array.
  CSA_ASSERT(this, Uint32LessThanOrEqual(var_low.value(), var_high.value()));

  Label binary_loop(this, {&var_high, &var_low});
  Goto(&binary_loop);
  BIND(&binary_loop);
  {
    // mid = low + (high - low) / 2 (to avoid overflow in "(low + high) / 2").
    TNode<Uint32T> mid = Unsigned(
        Int32Add(var_low.value(),
                 Word32Shr(Int32Sub(var_high.value(), var_low.value()), 1)));
    // mid_name = array->GetSortedKey(mid).
    TNode<Uint32T> sorted_key_index = GetSortedKeyIndex<Array>(array, mid);
    TNode<Name> mid_name = GetKey<Array>(array, sorted_key_index);

    TNode<Uint32T> mid_hash = LoadNameHashField(mid_name);

    Label mid_greater(this), mid_less(this), merge(this);
    Branch(Uint32GreaterThanOrEqual(mid_hash, hash), &mid_greater, &mid_less);
    BIND(&mid_greater);
    {
      var_high = mid;
      Goto(&merge);
    }
    BIND(&mid_less);
    {
      var_low = Unsigned(Int32Add(mid, Int32Constant(1)));
      Goto(&merge);
    }
    BIND(&merge);
    GotoIf(Word32NotEqual(var_low.value(), var_high.value()), &binary_loop);
  }

  Label scan_loop(this, &var_low);
  Goto(&scan_loop);
  BIND(&scan_loop);
  {
    GotoIf(Int32GreaterThan(var_low.value(), limit), if_not_found);

    TNode<Uint32T> sort_index =
        GetSortedKeyIndex<Array>(array, var_low.value());
    TNode<Name> current_name = GetKey<Array>(array, sort_index);
    TNode<Uint32T> current_hash = LoadNameHashField(current_name);
    GotoIf(Word32NotEqual(current_hash, hash), if_not_found);
    Label next(this);
    GotoIf(TaggedNotEqual(current_name, unique_name), &next);
    GotoIf(Uint32GreaterThanOrEqual(sort_index, number_of_valid_entries),
           if_not_found);
    *var_name_index = ToKeyIndex<Array>(sort_index);
    Goto(if_found);

    BIND(&next);
    var_low = Unsigned(Int32Add(var_low.value(), Int32Constant(1)));
    Goto(&scan_loop);
  }
}

void CodeStubAssembler::ForEachEnumerableOwnProperty(
    TNode<Context> context, TNode<Map> map, TNode<JSObject> object,
    ForEachEnumerationMode mode, const ForEachKeyValueFunction& body,
    Label* bailout) {
  TNode<Uint16T> type = LoadMapInstanceType(map);
  TNode<Uint32T> bit_field3 = EnsureOnlyHasSimpleProperties(map, type, bailout);

  TNode<DescriptorArray> descriptors = LoadMapDescriptors(map);
  TNode<Uint32T> nof_descriptors =
      DecodeWord32<Map::NumberOfOwnDescriptorsBits>(bit_field3);

  TVARIABLE(BoolT, var_stable, Int32TrueConstant());

  TVARIABLE(BoolT, var_has_symbol, Int32FalseConstant());
  // false - iterate only string properties, true - iterate only symbol
  // properties
  TVARIABLE(BoolT, var_is_symbol_processing_loop, Int32FalseConstant());
  TVARIABLE(IntPtrT, var_start_key_index,
            ToKeyIndex<DescriptorArray>(Unsigned(Int32Constant(0))));
  // Note: var_end_key_index is exclusive for the loop
  TVARIABLE(IntPtrT, var_end_key_index,
            ToKeyIndex<DescriptorArray>(nof_descriptors));
  VariableList list(
      {&var_stable, &var_has_symbol, &var_is_symbol_processing_loop,
       &var_start_key_index, &var_end_key_index},
      zone());
  Label descriptor_array_loop(
      this, {&var_stable, &var_has_symbol, &var_is_symbol_processing_loop,
             &var_start_key_index, &var_end_key_index});

  Goto(&descriptor_array_loop);
  BIND(&descriptor_array_loop);

  BuildFastLoop(
      list, var_start_key_index.value(), var_end_key_index.value(),
      [=, &var_stable, &var_has_symbol, &var_is_symbol_processing_loop,
       &var_start_key_index, &var_end_key_index](Node* index) {
        TNode<IntPtrT> descriptor_key_index =
            TNode<IntPtrT>::UncheckedCast(index);
        TNode<Name> next_key =
            LoadKeyByKeyIndex(descriptors, descriptor_key_index);

        TVARIABLE(Object, var_value, SmiConstant(0));
        Label callback(this), next_iteration(this);

        if (mode == kEnumerationOrder) {
          // |next_key| is either a string or a symbol
          // Skip strings or symbols depending on
          // |var_is_symbol_processing_loop|.
          Label if_string(this), if_symbol(this), if_name_ok(this);
          Branch(IsSymbol(next_key), &if_symbol, &if_string);
          BIND(&if_symbol);
          {
            // Process symbol property when |var_is_symbol_processing_loop| is
            // true.
            GotoIf(var_is_symbol_processing_loop.value(), &if_name_ok);
            // First iteration need to calculate smaller range for processing
            // symbols
            Label if_first_symbol(this);
            // var_end_key_index is still inclusive at this point.
            var_end_key_index = descriptor_key_index;
            Branch(var_has_symbol.value(), &next_iteration, &if_first_symbol);
            BIND(&if_first_symbol);
            {
              var_start_key_index = descriptor_key_index;
              var_has_symbol = Int32TrueConstant();
              Goto(&next_iteration);
            }
          }
          BIND(&if_string);
          {
            CSA_ASSERT(this, IsString(next_key));
            // Process string property when |var_is_symbol_processing_loop| is
            // false.
            Branch(var_is_symbol_processing_loop.value(), &next_iteration,
                   &if_name_ok);
          }
          BIND(&if_name_ok);
        }
        {
          TVARIABLE(Map, var_map);
          TVARIABLE(HeapObject, var_meta_storage);
          TVARIABLE(IntPtrT, var_entry);
          TVARIABLE(Uint32T, var_details);
          Label if_found(this);

          Label if_found_fast(this), if_found_dict(this);

          Label if_stable(this), if_not_stable(this);
          Branch(var_stable.value(), &if_stable, &if_not_stable);
          BIND(&if_stable);
          {
            // Directly decode from the descriptor array if |object| did not
            // change shape.
            var_map = map;
            var_meta_storage = descriptors;
            var_entry = Signed(descriptor_key_index);
            Goto(&if_found_fast);
          }
          BIND(&if_not_stable);
          {
            // If the map did change, do a slower lookup. We are still
            // guaranteed that the object has a simple shape, and that the key
            // is a name.
            var_map = LoadMap(object);
            TryLookupPropertyInSimpleObject(
                object, var_map.value(), next_key, &if_found_fast,
                &if_found_dict, &var_meta_storage, &var_entry, &next_iteration);
          }

          BIND(&if_found_fast);
          {
            TNode<DescriptorArray> descriptors = CAST(var_meta_storage.value());
            TNode<IntPtrT> name_index = var_entry.value();

            // Skip non-enumerable properties.
            var_details = LoadDetailsByKeyIndex(descriptors, name_index);
            GotoIf(IsSetWord32(var_details.value(),
                               PropertyDetails::kAttributesDontEnumMask),
                   &next_iteration);

            LoadPropertyFromFastObject(object, var_map.value(), descriptors,
                                       name_index, var_details.value(),
                                       &var_value);
            Goto(&if_found);
          }
          BIND(&if_found_dict);
          {
            TNode<NameDictionary> dictionary = CAST(var_meta_storage.value());
            TNode<IntPtrT> entry = var_entry.value();

            TNode<Uint32T> details =
                LoadDetailsByKeyIndex<NameDictionary>(dictionary, entry);
            // Skip non-enumerable properties.
            GotoIf(
                IsSetWord32(details, PropertyDetails::kAttributesDontEnumMask),
                &next_iteration);

            var_details = details;
            var_value = LoadValueByKeyIndex<NameDictionary>(dictionary, entry);
            Goto(&if_found);
          }

          // Here we have details and value which could be an accessor.
          BIND(&if_found);
          {
            Label slow_load(this, Label::kDeferred);

            var_value = CallGetterIfAccessor(var_value.value(),
                                             var_details.value(), context,
                                             object, &slow_load, kCallJSGetter);
            Goto(&callback);

            BIND(&slow_load);
            var_value =
                CallRuntime(Runtime::kGetProperty, context, object, next_key);
            Goto(&callback);

            BIND(&callback);
            body(next_key, var_value.value());

            // Check if |object| is still stable, i.e. we can proceed using
            // property details from preloaded |descriptors|.
            var_stable = Select<BoolT>(
                var_stable.value(),
                [=] { return TaggedEqual(LoadMap(object), map); },
                [=] { return Int32FalseConstant(); });

            Goto(&next_iteration);
          }
        }
        BIND(&next_iteration);
      },
      DescriptorArray::kEntrySize, INTPTR_PARAMETERS, IndexAdvanceMode::kPost);

  if (mode == kEnumerationOrder) {
    Label done(this);
    GotoIf(var_is_symbol_processing_loop.value(), &done);
    GotoIfNot(var_has_symbol.value(), &done);
    // All string properties are processed, now process symbol properties.
    var_is_symbol_processing_loop = Int32TrueConstant();
    // Add DescriptorArray::kEntrySize to make the var_end_key_index exclusive
    // as BuildFastLoop() expects.
    Increment(&var_end_key_index, DescriptorArray::kEntrySize,
              INTPTR_PARAMETERS);
    Goto(&descriptor_array_loop);

    BIND(&done);
  }
}

void CodeStubAssembler::DescriptorLookup(
    SloppyTNode<Name> unique_name, SloppyTNode<DescriptorArray> descriptors,
    SloppyTNode<Uint32T> bitfield3, Label* if_found,
    TVariable<IntPtrT>* var_name_index, Label* if_not_found) {
  Comment("DescriptorArrayLookup");
  TNode<Uint32T> nof = DecodeWord32<Map::NumberOfOwnDescriptorsBits>(bitfield3);
  Lookup<DescriptorArray>(unique_name, descriptors, nof, if_found,
                          var_name_index, if_not_found);
}

void CodeStubAssembler::TransitionLookup(
    SloppyTNode<Name> unique_name, SloppyTNode<TransitionArray> transitions,
    Label* if_found, TVariable<IntPtrT>* var_name_index, Label* if_not_found) {
  Comment("TransitionArrayLookup");
  TNode<Uint32T> number_of_valid_transitions =
      NumberOfEntries<TransitionArray>(transitions);
  Lookup<TransitionArray>(unique_name, transitions, number_of_valid_transitions,
                          if_found, var_name_index, if_not_found);
}

template <typename Array>
void CodeStubAssembler::Lookup(TNode<Name> unique_name, TNode<Array> array,
                               TNode<Uint32T> number_of_valid_entries,
                               Label* if_found,
                               TVariable<IntPtrT>* var_name_index,
                               Label* if_not_found) {
  Comment("ArrayLookup");
  if (!number_of_valid_entries) {
    number_of_valid_entries = NumberOfEntries(array);
  }
  GotoIf(Word32Equal(number_of_valid_entries, Int32Constant(0)), if_not_found);
  Label linear_search(this), binary_search(this);
  const int kMaxElementsForLinearSearch = 32;
  Branch(Uint32LessThanOrEqual(number_of_valid_entries,
                               Int32Constant(kMaxElementsForLinearSearch)),
         &linear_search, &binary_search);
  BIND(&linear_search);
  {
    LookupLinear<Array>(unique_name, array, number_of_valid_entries, if_found,
                        var_name_index, if_not_found);
  }
  BIND(&binary_search);
  {
    LookupBinary<Array>(unique_name, array, number_of_valid_entries, if_found,
                        var_name_index, if_not_found);
  }
}

TNode<BoolT> CodeStubAssembler::IsSimpleObjectMap(TNode<Map> map) {
  uint32_t mask =
      Map::HasNamedInterceptorBit::kMask | Map::IsAccessCheckNeededBit::kMask;
  // !IsSpecialReceiverType && !IsNamedInterceptor && !IsAccessCheckNeeded
  return Select<BoolT>(
      IsSpecialReceiverInstanceType(LoadMapInstanceType(map)),
      [=] { return Int32FalseConstant(); },
      [=] { return IsClearWord32(LoadMapBitField(map), mask); });
}

void CodeStubAssembler::TryLookupPropertyInSimpleObject(
    TNode<JSObject> object, TNode<Map> map, TNode<Name> unique_name,
    Label* if_found_fast, Label* if_found_dict,
    TVariable<HeapObject>* var_meta_storage, TVariable<IntPtrT>* var_name_index,
    Label* if_not_found) {
  CSA_ASSERT(this, IsSimpleObjectMap(map));
  CSA_ASSERT(this, IsUniqueNameNoIndex(unique_name));

  TNode<Uint32T> bit_field3 = LoadMapBitField3(map);
  Label if_isfastmap(this), if_isslowmap(this);
  Branch(IsSetWord32<Map::IsDictionaryMapBit>(bit_field3), &if_isslowmap,
         &if_isfastmap);
  BIND(&if_isfastmap);
  {
    TNode<DescriptorArray> descriptors = LoadMapDescriptors(map);
    *var_meta_storage = descriptors;

    DescriptorLookup(unique_name, descriptors, bit_field3, if_found_fast,
                     var_name_index, if_not_found);
  }
  BIND(&if_isslowmap);
  {
    TNode<NameDictionary> dictionary = CAST(LoadSlowProperties(object));
    *var_meta_storage = dictionary;

    NameDictionaryLookup<NameDictionary>(dictionary, unique_name, if_found_dict,
                                         var_name_index, if_not_found);
  }
}

void CodeStubAssembler::TryLookupProperty(
    SloppyTNode<JSObject> object, SloppyTNode<Map> map,
    SloppyTNode<Int32T> instance_type, SloppyTNode<Name> unique_name,
    Label* if_found_fast, Label* if_found_dict, Label* if_found_global,
    TVariable<HeapObject>* var_meta_storage, TVariable<IntPtrT>* var_name_index,
    Label* if_not_found, Label* if_bailout) {
  Label if_objectisspecial(this);
  GotoIf(IsSpecialReceiverInstanceType(instance_type), &if_objectisspecial);

  TryLookupPropertyInSimpleObject(object, map, unique_name, if_found_fast,
                                  if_found_dict, var_meta_storage,
                                  var_name_index, if_not_found);

  BIND(&if_objectisspecial);
  {
    // Handle global object here and bailout for other special objects.
    GotoIfNot(InstanceTypeEqual(instance_type, JS_GLOBAL_OBJECT_TYPE),
              if_bailout);

    // Handle interceptors and access checks in runtime.
    TNode<Int32T> bit_field = LoadMapBitField(map);
    int mask =
        Map::HasNamedInterceptorBit::kMask | Map::IsAccessCheckNeededBit::kMask;
    GotoIf(IsSetWord32(bit_field, mask), if_bailout);

    TNode<GlobalDictionary> dictionary = CAST(LoadSlowProperties(object));
    *var_meta_storage = dictionary;

    NameDictionaryLookup<GlobalDictionary>(
        dictionary, unique_name, if_found_global, var_name_index, if_not_found);
  }
}

void CodeStubAssembler::TryHasOwnProperty(Node* object, Node* map,
                                          Node* instance_type,
                                          Node* unique_name, Label* if_found,
                                          Label* if_not_found,
                                          Label* if_bailout) {
  Comment("TryHasOwnProperty");
  CSA_ASSERT(this, IsUniqueNameNoIndex(CAST(unique_name)));
  TVARIABLE(HeapObject, var_meta_storage);
  TVARIABLE(IntPtrT, var_name_index);

  Label if_found_global(this);
  TryLookupProperty(object, map, instance_type, unique_name, if_found, if_found,
                    &if_found_global, &var_meta_storage, &var_name_index,
                    if_not_found, if_bailout);

  BIND(&if_found_global);
  {
    VARIABLE(var_value, MachineRepresentation::kTagged);
    VARIABLE(var_details, MachineRepresentation::kWord32);
    // Check if the property cell is not deleted.
    LoadPropertyFromGlobalDictionary(var_meta_storage.value(),
                                     var_name_index.value(), &var_value,
                                     &var_details, if_not_found);
    Goto(if_found);
  }
}

Node* CodeStubAssembler::GetMethod(Node* context, Node* object,
                                   Handle<Name> name,
                                   Label* if_null_or_undefined) {
  TNode<Object> method = GetProperty(context, object, name);

  GotoIf(IsUndefined(method), if_null_or_undefined);
  GotoIf(IsNull(method), if_null_or_undefined);

  return method;
}

TNode<Object> CodeStubAssembler::GetIteratorMethod(
    TNode<Context> context, TNode<HeapObject> heap_obj,
    Label* if_iteratorundefined) {
  return CAST(GetMethod(context, heap_obj,
                        isolate()->factory()->iterator_symbol(),
                        if_iteratorundefined));
}

void CodeStubAssembler::LoadPropertyFromFastObject(
    Node* object, Node* map, TNode<DescriptorArray> descriptors,
    Node* name_index, Variable* var_details, Variable* var_value) {
  DCHECK_EQ(MachineRepresentation::kWord32, var_details->rep());
  DCHECK_EQ(MachineRepresentation::kTagged, var_value->rep());

  TNode<Uint32T> details =
      LoadDetailsByKeyIndex(descriptors, UncheckedCast<IntPtrT>(name_index));
  var_details->Bind(details);

  LoadPropertyFromFastObject(object, map, descriptors, name_index, details,
                             var_value);
}

void CodeStubAssembler::LoadPropertyFromFastObject(
    Node* object, Node* map, TNode<DescriptorArray> descriptors,
    Node* name_index, Node* details, Variable* var_value) {
  Comment("[ LoadPropertyFromFastObject");

  TNode<Uint32T> location =
      DecodeWord32<PropertyDetails::LocationField>(details);

  Label if_in_field(this), if_in_descriptor(this), done(this);
  Branch(Word32Equal(location, Int32Constant(kField)), &if_in_field,
         &if_in_descriptor);
  BIND(&if_in_field);
  {
    TNode<IntPtrT> field_index =
        Signed(DecodeWordFromWord32<PropertyDetails::FieldIndexField>(details));
    TNode<Uint32T> representation =
        DecodeWord32<PropertyDetails::RepresentationField>(details);

    field_index =
        IntPtrAdd(field_index, LoadMapInobjectPropertiesStartInWords(map));
    TNode<IntPtrT> instance_size_in_words = LoadMapInstanceSizeInWords(map);

    Label if_inobject(this), if_backing_store(this);
    VARIABLE(var_double_value, MachineRepresentation::kFloat64);
    Label rebox_double(this, &var_double_value);
    Branch(UintPtrLessThan(field_index, instance_size_in_words), &if_inobject,
           &if_backing_store);
    BIND(&if_inobject);
    {
      Comment("if_inobject");
      TNode<IntPtrT> field_offset = TimesTaggedSize(field_index);

      Label if_double(this), if_tagged(this);
      Branch(Word32NotEqual(representation,
                            Int32Constant(Representation::kDouble)),
             &if_tagged, &if_double);
      BIND(&if_tagged);
      {
        var_value->Bind(LoadObjectField(object, field_offset));
        Goto(&done);
      }
      BIND(&if_double);
      {
        if (FLAG_unbox_double_fields) {
          var_double_value.Bind(
              LoadObjectField(object, field_offset, MachineType::Float64()));
        } else {
          TNode<HeapNumber> heap_number =
              CAST(LoadObjectField(object, field_offset));
          var_double_value.Bind(LoadHeapNumberValue(heap_number));
        }
        Goto(&rebox_double);
      }
    }
    BIND(&if_backing_store);
    {
      Comment("if_backing_store");
      TNode<HeapObject> properties = LoadFastProperties(object);
      field_index = Signed(IntPtrSub(field_index, instance_size_in_words));
      TNode<Object> value =
          LoadPropertyArrayElement(CAST(properties), field_index);

      Label if_double(this), if_tagged(this);
      Branch(Word32NotEqual(representation,
                            Int32Constant(Representation::kDouble)),
             &if_tagged, &if_double);
      BIND(&if_tagged);
      {
        var_value->Bind(value);
        Goto(&done);
      }
      BIND(&if_double);
      {
        var_double_value.Bind(LoadHeapNumberValue(CAST(value)));
        Goto(&rebox_double);
      }
    }
    BIND(&rebox_double);
    {
      Comment("rebox_double");
      TNode<HeapNumber> heap_number =
          AllocateHeapNumberWithValue(var_double_value.value());
      var_value->Bind(heap_number);
      Goto(&done);
    }
  }
  BIND(&if_in_descriptor);
  {
    var_value->Bind(
        LoadValueByKeyIndex(descriptors, UncheckedCast<IntPtrT>(name_index)));
    Goto(&done);
  }
  BIND(&done);

  Comment("] LoadPropertyFromFastObject");
}

void CodeStubAssembler::LoadPropertyFromNameDictionary(Node* dictionary,
                                                       Node* name_index,
                                                       Variable* var_details,
                                                       Variable* var_value) {
  Comment("LoadPropertyFromNameDictionary");
  CSA_ASSERT(this, IsNameDictionary(dictionary));

  var_details->Bind(
      LoadDetailsByKeyIndex<NameDictionary>(dictionary, name_index));
  var_value->Bind(LoadValueByKeyIndex<NameDictionary>(dictionary, name_index));

  Comment("] LoadPropertyFromNameDictionary");
}

void CodeStubAssembler::LoadPropertyFromGlobalDictionary(Node* dictionary,
                                                         Node* name_index,
                                                         Variable* var_details,
                                                         Variable* var_value,
                                                         Label* if_deleted) {
  Comment("[ LoadPropertyFromGlobalDictionary");
  CSA_ASSERT(this, IsGlobalDictionary(dictionary));

  TNode<PropertyCell> property_cell =
      CAST(LoadFixedArrayElement(CAST(dictionary), name_index));

  TNode<Object> value =
      LoadObjectField(property_cell, PropertyCell::kValueOffset);
  GotoIf(TaggedEqual(value, TheHoleConstant()), if_deleted);

  var_value->Bind(value);

  TNode<Int32T> details = LoadAndUntagToWord32ObjectField(
      property_cell, PropertyCell::kPropertyDetailsRawOffset);
  var_details->Bind(details);

  Comment("] LoadPropertyFromGlobalDictionary");
}

// |value| is the property backing store's contents, which is either a value
// or an accessor pair, as specified by |details|.
// Returns either the original value, or the result of the getter call.
TNode<Object> CodeStubAssembler::CallGetterIfAccessor(
    Node* value, Node* details, Node* context, Node* receiver,
    Label* if_bailout, GetOwnPropertyMode mode) {
  VARIABLE(var_value, MachineRepresentation::kTagged, value);
  Label done(this), if_accessor_info(this, Label::kDeferred);

  TNode<Uint32T> kind = DecodeWord32<PropertyDetails::KindField>(details);
  GotoIf(Word32Equal(kind, Int32Constant(kData)), &done);

  // Accessor case.
  GotoIfNot(IsAccessorPair(value), &if_accessor_info);

  // AccessorPair case.
  {
    if (mode == kCallJSGetter) {
      Node* accessor_pair = value;
      TNode<HeapObject> getter =
          CAST(LoadObjectField(accessor_pair, AccessorPair::kGetterOffset));
      TNode<Map> getter_map = LoadMap(getter);
      TNode<Uint16T> instance_type = LoadMapInstanceType(getter_map);
      // FunctionTemplateInfo getters are not supported yet.
      GotoIf(InstanceTypeEqual(instance_type, FUNCTION_TEMPLATE_INFO_TYPE),
             if_bailout);

      // Return undefined if the {getter} is not callable.
      var_value.Bind(UndefinedConstant());
      GotoIfNot(IsCallableMap(getter_map), &done);

      // Call the accessor.
      Callable callable = CodeFactory::Call(isolate());
      Node* result = CallJS(callable, context, getter, receiver);
      var_value.Bind(result);
    }
    Goto(&done);
  }

  // AccessorInfo case.
  BIND(&if_accessor_info);
  {
    Node* accessor_info = value;
    CSA_ASSERT(this, IsAccessorInfo(value));
    CSA_ASSERT(this, TaggedIsNotSmi(receiver));
    Label if_array(this), if_function(this), if_wrapper(this);

    // Dispatch based on {receiver} instance type.
    TNode<Map> receiver_map = LoadMap(receiver);
    TNode<Uint16T> receiver_instance_type = LoadMapInstanceType(receiver_map);
    GotoIf(IsJSArrayInstanceType(receiver_instance_type), &if_array);
    GotoIf(IsJSFunctionInstanceType(receiver_instance_type), &if_function);
    Branch(IsJSPrimitiveWrapperInstanceType(receiver_instance_type),
           &if_wrapper, if_bailout);

    // JSArray AccessorInfo case.
    BIND(&if_array);
    {
      // We only deal with the "length" accessor on JSArray.
      GotoIfNot(IsLengthString(
                    LoadObjectField(accessor_info, AccessorInfo::kNameOffset)),
                if_bailout);
      var_value.Bind(LoadJSArrayLength(receiver));
      Goto(&done);
    }

    // JSFunction AccessorInfo case.
    BIND(&if_function);
    {
      // We only deal with the "prototype" accessor on JSFunction here.
      GotoIfNot(IsPrototypeString(
                    LoadObjectField(accessor_info, AccessorInfo::kNameOffset)),
                if_bailout);

      GotoIfPrototypeRequiresRuntimeLookup(CAST(receiver), receiver_map,
                                           if_bailout);
      var_value.Bind(LoadJSFunctionPrototype(CAST(receiver), if_bailout));
      Goto(&done);
    }

    // JSPrimitiveWrapper AccessorInfo case.
    BIND(&if_wrapper);
    {
      // We only deal with the "length" accessor on JSPrimitiveWrapper string
      // wrappers.
      GotoIfNot(IsLengthString(
                    LoadObjectField(accessor_info, AccessorInfo::kNameOffset)),
                if_bailout);
      Node* receiver_value = LoadJSPrimitiveWrapperValue(receiver);
      GotoIfNot(TaggedIsNotSmi(receiver_value), if_bailout);
      GotoIfNot(IsString(receiver_value), if_bailout);
      var_value.Bind(LoadStringLengthAsSmi(receiver_value));
      Goto(&done);
    }
  }

  BIND(&done);
  return UncheckedCast<Object>(var_value.value());
}

void CodeStubAssembler::TryGetOwnProperty(
    Node* context, Node* receiver, Node* object, Node* map, Node* instance_type,
    Node* unique_name, Label* if_found_value, Variable* var_value,
    Label* if_not_found, Label* if_bailout) {
  TryGetOwnProperty(context, receiver, object, map, instance_type, unique_name,
                    if_found_value, var_value, nullptr, nullptr, if_not_found,
                    if_bailout, kCallJSGetter);
}

void CodeStubAssembler::TryGetOwnProperty(
    Node* context, Node* receiver, Node* object, Node* map, Node* instance_type,
    Node* unique_name, Label* if_found_value, Variable* var_value,
    Variable* var_details, Variable* var_raw_value, Label* if_not_found,
    Label* if_bailout, GetOwnPropertyMode mode) {
  DCHECK_EQ(MachineRepresentation::kTagged, var_value->rep());
  Comment("TryGetOwnProperty");
  CSA_ASSERT(this, IsUniqueNameNoIndex(CAST(unique_name)));

  TVARIABLE(HeapObject, var_meta_storage);
  TVARIABLE(IntPtrT, var_entry);

  Label if_found_fast(this), if_found_dict(this), if_found_global(this);

  VARIABLE(local_var_details, MachineRepresentation::kWord32);
  if (!var_details) {
    var_details = &local_var_details;
  }
  Label if_found(this);

  TryLookupProperty(object, map, instance_type, unique_name, &if_found_fast,
                    &if_found_dict, &if_found_global, &var_meta_storage,
                    &var_entry, if_not_found, if_bailout);
  BIND(&if_found_fast);
  {
    TNode<DescriptorArray> descriptors = CAST(var_meta_storage.value());
    TNode<IntPtrT> name_index = var_entry.value();

    LoadPropertyFromFastObject(object, map, descriptors, name_index,
                               var_details, var_value);
    Goto(&if_found);
  }
  BIND(&if_found_dict);
  {
    TNode<HeapObject> dictionary = var_meta_storage.value();
    TNode<IntPtrT> entry = var_entry.value();
    LoadPropertyFromNameDictionary(dictionary, entry, var_details, var_value);
    Goto(&if_found);
  }
  BIND(&if_found_global);
  {
    TNode<HeapObject> dictionary = var_meta_storage.value();
    TNode<IntPtrT> entry = var_entry.value();

    LoadPropertyFromGlobalDictionary(dictionary, entry, var_details, var_value,
                                     if_not_found);
    Goto(&if_found);
  }
  // Here we have details and value which could be an accessor.
  BIND(&if_found);
  {
    // TODO(ishell): Execute C++ accessor in case of accessor info
    if (var_raw_value) {
      var_raw_value->Bind(var_value->value());
    }
    TNode<Object> value =
        CallGetterIfAccessor(var_value->value(), var_details->value(), context,
                             receiver, if_bailout, mode);
    var_value->Bind(value);
    Goto(if_found_value);
  }
}

void CodeStubAssembler::TryLookupElement(Node* object, Node* map,
                                         SloppyTNode<Int32T> instance_type,
                                         SloppyTNode<IntPtrT> intptr_index,
                                         Label* if_found, Label* if_absent,
                                         Label* if_not_found,
                                         Label* if_bailout) {
  // Handle special objects in runtime.
  GotoIf(IsSpecialReceiverInstanceType(instance_type), if_bailout);

  TNode<Int32T> elements_kind = LoadMapElementsKind(map);

  // TODO(verwaest): Support other elements kinds as well.
  Label if_isobjectorsmi(this), if_isdouble(this), if_isdictionary(this),
      if_isfaststringwrapper(this), if_isslowstringwrapper(this), if_oob(this),
      if_typedarray(this);
  // clang-format off
  int32_t values[] = {
      // Handled by {if_isobjectorsmi}.
      PACKED_SMI_ELEMENTS, HOLEY_SMI_ELEMENTS, PACKED_ELEMENTS, HOLEY_ELEMENTS,
      PACKED_NONEXTENSIBLE_ELEMENTS, PACKED_SEALED_ELEMENTS,
      HOLEY_NONEXTENSIBLE_ELEMENTS, HOLEY_SEALED_ELEMENTS,
      PACKED_FROZEN_ELEMENTS, HOLEY_FROZEN_ELEMENTS,
      // Handled by {if_isdouble}.
      PACKED_DOUBLE_ELEMENTS, HOLEY_DOUBLE_ELEMENTS,
      // Handled by {if_isdictionary}.
      DICTIONARY_ELEMENTS,
      // Handled by {if_isfaststringwrapper}.
      FAST_STRING_WRAPPER_ELEMENTS,
      // Handled by {if_isslowstringwrapper}.
      SLOW_STRING_WRAPPER_ELEMENTS,
      // Handled by {if_not_found}.
      NO_ELEMENTS,
      // Handled by {if_typed_array}.
      UINT8_ELEMENTS,
      INT8_ELEMENTS,
      UINT16_ELEMENTS,
      INT16_ELEMENTS,
      UINT32_ELEMENTS,
      INT32_ELEMENTS,
      FLOAT32_ELEMENTS,
      FLOAT64_ELEMENTS,
      UINT8_CLAMPED_ELEMENTS,
      BIGUINT64_ELEMENTS,
      BIGINT64_ELEMENTS,
  };
  Label* labels[] = {
      &if_isobjectorsmi, &if_isobjectorsmi, &if_isobjectorsmi,
      &if_isobjectorsmi, &if_isobjectorsmi, &if_isobjectorsmi,
      &if_isobjectorsmi, &if_isobjectorsmi, &if_isobjectorsmi,
      &if_isobjectorsmi,
      &if_isdouble, &if_isdouble,
      &if_isdictionary,
      &if_isfaststringwrapper,
      &if_isslowstringwrapper,
      if_not_found,
      &if_typedarray,
      &if_typedarray,
      &if_typedarray,
      &if_typedarray,
      &if_typedarray,
      &if_typedarray,
      &if_typedarray,
      &if_typedarray,
      &if_typedarray,
      &if_typedarray,
      &if_typedarray,
  };
  // clang-format on
  STATIC_ASSERT(arraysize(values) == arraysize(labels));
  Switch(elements_kind, if_bailout, values, labels, arraysize(values));

  BIND(&if_isobjectorsmi);
  {
    TNode<FixedArray> elements = CAST(LoadElements(object));
    TNode<IntPtrT> length = LoadAndUntagFixedArrayBaseLength(elements);

    GotoIfNot(UintPtrLessThan(intptr_index, length), &if_oob);

    TNode<Object> element = UnsafeLoadFixedArrayElement(elements, intptr_index);
    TNode<Oddball> the_hole = TheHoleConstant();
    Branch(TaggedEqual(element, the_hole), if_not_found, if_found);
  }
  BIND(&if_isdouble);
  {
    TNode<FixedArrayBase> elements = LoadElements(object);
    TNode<IntPtrT> length = LoadAndUntagFixedArrayBaseLength(elements);

    GotoIfNot(UintPtrLessThan(intptr_index, length), &if_oob);

    // Check if the element is a double hole, but don't load it.
    LoadFixedDoubleArrayElement(CAST(elements), intptr_index,
                                MachineType::None(), 0, INTPTR_PARAMETERS,
                                if_not_found);
    Goto(if_found);
  }
  BIND(&if_isdictionary);
  {
    // Negative keys must be converted to property names.
    GotoIf(IntPtrLessThan(intptr_index, IntPtrConstant(0)), if_bailout);

    TVARIABLE(IntPtrT, var_entry);
    TNode<NumberDictionary> elements = CAST(LoadElements(object));
    NumberDictionaryLookup(elements, intptr_index, if_found, &var_entry,
                           if_not_found);
  }
  BIND(&if_isfaststringwrapper);
  {
    CSA_ASSERT(this, HasInstanceType(object, JS_PRIMITIVE_WRAPPER_TYPE));
    Node* string = LoadJSPrimitiveWrapperValue(object);
    CSA_ASSERT(this, IsString(string));
    TNode<IntPtrT> length = LoadStringLengthAsWord(string);
    GotoIf(UintPtrLessThan(intptr_index, length), if_found);
    Goto(&if_isobjectorsmi);
  }
  BIND(&if_isslowstringwrapper);
  {
    CSA_ASSERT(this, HasInstanceType(object, JS_PRIMITIVE_WRAPPER_TYPE));
    Node* string = LoadJSPrimitiveWrapperValue(object);
    CSA_ASSERT(this, IsString(string));
    TNode<IntPtrT> length = LoadStringLengthAsWord(string);
    GotoIf(UintPtrLessThan(intptr_index, length), if_found);
    Goto(&if_isdictionary);
  }
  BIND(&if_typedarray);
  {
    TNode<JSArrayBuffer> buffer = LoadJSArrayBufferViewBuffer(CAST(object));
    GotoIf(IsDetachedBuffer(buffer), if_absent);

    TNode<UintPtrT> length = LoadJSTypedArrayLength(CAST(object));
    Branch(UintPtrLessThan(intptr_index, length), if_found, if_absent);
  }
  BIND(&if_oob);
  {
    // Positive OOB indices mean "not found", negative indices must be
    // converted to property names.
    GotoIf(IntPtrLessThan(intptr_index, IntPtrConstant(0)), if_bailout);
    Goto(if_not_found);
  }
}

void CodeStubAssembler::BranchIfMaybeSpecialIndex(TNode<String> name_string,
                                                  Label* if_maybe_special_index,
                                                  Label* if_not_special_index) {
  // TODO(cwhan.tunz): Implement fast cases more.

  // If a name is empty or too long, it's not a special index
  // Max length of canonical double: -X.XXXXXXXXXXXXXXXXX-eXXX
  const int kBufferSize = 24;
  TNode<Smi> string_length = LoadStringLengthAsSmi(name_string);
  GotoIf(SmiEqual(string_length, SmiConstant(0)), if_not_special_index);
  GotoIf(SmiGreaterThan(string_length, SmiConstant(kBufferSize)),
         if_not_special_index);

  // If the first character of name is not a digit or '-', or we can't match it
  // to Infinity or NaN, then this is not a special index.
  TNode<Int32T> first_char = StringCharCodeAt(name_string, IntPtrConstant(0));
  // If the name starts with '-', it can be a negative index.
  GotoIf(Word32Equal(first_char, Int32Constant('-')), if_maybe_special_index);
  // If the name starts with 'I', it can be "Infinity".
  GotoIf(Word32Equal(first_char, Int32Constant('I')), if_maybe_special_index);
  // If the name starts with 'N', it can be "NaN".
  GotoIf(Word32Equal(first_char, Int32Constant('N')), if_maybe_special_index);
  // Finally, if the first character is not a digit either, then we are sure
  // that the name is not a special index.
  GotoIf(Uint32LessThan(first_char, Int32Constant('0')), if_not_special_index);
  GotoIf(Uint32LessThan(Int32Constant('9'), first_char), if_not_special_index);
  Goto(if_maybe_special_index);
}

void CodeStubAssembler::TryPrototypeChainLookup(
    Node* receiver, Node* object, Node* key,
    const LookupInHolder& lookup_property_in_holder,
    const LookupInHolder& lookup_element_in_holder, Label* if_end,
    Label* if_bailout, Label* if_proxy) {
  // Ensure receiver is JSReceiver, otherwise bailout.
  GotoIf(TaggedIsSmi(receiver), if_bailout);
  CSA_ASSERT(this, TaggedIsNotSmi(object));

  TNode<Map> map = LoadMap(object);
  TNode<Uint16T> instance_type = LoadMapInstanceType(map);
  {
    Label if_objectisreceiver(this);
    STATIC_ASSERT(LAST_JS_RECEIVER_TYPE == LAST_TYPE);
    STATIC_ASSERT(FIRST_JS_RECEIVER_TYPE == JS_PROXY_TYPE);
    Branch(IsJSReceiverInstanceType(instance_type), &if_objectisreceiver,
           if_bailout);
    BIND(&if_objectisreceiver);

    GotoIf(InstanceTypeEqual(instance_type, JS_PROXY_TYPE), if_proxy);
  }

  VARIABLE(var_index, MachineType::PointerRepresentation());
  VARIABLE(var_unique, MachineRepresentation::kTagged);

  Label if_keyisindex(this), if_iskeyunique(this);
  TryToName(key, &if_keyisindex, &var_index, &if_iskeyunique, &var_unique,
            if_bailout);

  BIND(&if_iskeyunique);
  {
    TVARIABLE(HeapObject, var_holder, CAST(object));
    TVARIABLE(Map, var_holder_map, map);
    TVARIABLE(Int32T, var_holder_instance_type, instance_type);

    VariableList merged_variables(
        {&var_holder, &var_holder_map, &var_holder_instance_type}, zone());
    Label loop(this, merged_variables);
    Goto(&loop);
    BIND(&loop);
    {
      TNode<Map> holder_map = var_holder_map.value();
      TNode<Int32T> holder_instance_type = var_holder_instance_type.value();

      Label next_proto(this), check_integer_indexed_exotic(this);
      lookup_property_in_holder(receiver, var_holder.value(), holder_map,
                                holder_instance_type, var_unique.value(),
                                &check_integer_indexed_exotic, if_bailout);

      BIND(&check_integer_indexed_exotic);
      {
        // Bailout if it can be an integer indexed exotic case.
        GotoIfNot(InstanceTypeEqual(holder_instance_type, JS_TYPED_ARRAY_TYPE),
                  &next_proto);
        GotoIfNot(IsString(var_unique.value()), &next_proto);
        BranchIfMaybeSpecialIndex(CAST(var_unique.value()), if_bailout,
                                  &next_proto);
      }

      BIND(&next_proto);

      TNode<HeapObject> proto = LoadMapPrototype(holder_map);

      GotoIf(IsNull(proto), if_end);

      TNode<Map> map = LoadMap(proto);
      TNode<Uint16T> instance_type = LoadMapInstanceType(map);

      var_holder = proto;
      var_holder_map = map;
      var_holder_instance_type = instance_type;
      Goto(&loop);
    }
  }
  BIND(&if_keyisindex);
  {
    TVARIABLE(HeapObject, var_holder, CAST(object));
    TVARIABLE(Map, var_holder_map, map);
    TVARIABLE(Int32T, var_holder_instance_type, instance_type);

    VariableList merged_variables(
        {&var_holder, &var_holder_map, &var_holder_instance_type}, zone());
    Label loop(this, merged_variables);
    Goto(&loop);
    BIND(&loop);
    {
      Label next_proto(this);
      lookup_element_in_holder(receiver, var_holder.value(),
                               var_holder_map.value(),
                               var_holder_instance_type.value(),
                               var_index.value(), &next_proto, if_bailout);
      BIND(&next_proto);

      TNode<HeapObject> proto = LoadMapPrototype(var_holder_map.value());

      GotoIf(IsNull(proto), if_end);

      TNode<Map> map = LoadMap(proto);
      TNode<Uint16T> instance_type = LoadMapInstanceType(map);

      var_holder = proto;
      var_holder_map = map;
      var_holder_instance_type = instance_type;
      Goto(&loop);
    }
  }
}

Node* CodeStubAssembler::HasInPrototypeChain(Node* context, Node* object,
                                             SloppyTNode<Object> prototype) {
  CSA_ASSERT(this, TaggedIsNotSmi(object));
  VARIABLE(var_result, MachineRepresentation::kTagged);
  Label return_false(this), return_true(this),
      return_runtime(this, Label::kDeferred), return_result(this);

  // Loop through the prototype chain looking for the {prototype}.
  VARIABLE(var_object_map, MachineRepresentation::kTagged, LoadMap(object));
  Label loop(this, &var_object_map);
  Goto(&loop);
  BIND(&loop);
  {
    // Check if we can determine the prototype directly from the {object_map}.
    Label if_objectisdirect(this), if_objectisspecial(this, Label::kDeferred);
    Node* object_map = var_object_map.value();
    TNode<Uint16T> object_instance_type = LoadMapInstanceType(object_map);
    Branch(IsSpecialReceiverInstanceType(object_instance_type),
           &if_objectisspecial, &if_objectisdirect);
    BIND(&if_objectisspecial);
    {
      // The {object_map} is a special receiver map or a primitive map, check
      // if we need to use the if_objectisspecial path in the runtime.
      GotoIf(InstanceTypeEqual(object_instance_type, JS_PROXY_TYPE),
             &return_runtime);
      TNode<Int32T> object_bitfield = LoadMapBitField(object_map);
      int mask = Map::HasNamedInterceptorBit::kMask |
                 Map::IsAccessCheckNeededBit::kMask;
      Branch(IsSetWord32(object_bitfield, mask), &return_runtime,
             &if_objectisdirect);
    }
    BIND(&if_objectisdirect);

    // Check the current {object} prototype.
    TNode<HeapObject> object_prototype = LoadMapPrototype(object_map);
    GotoIf(IsNull(object_prototype), &return_false);
    GotoIf(TaggedEqual(object_prototype, prototype), &return_true);

    // Continue with the prototype.
    CSA_ASSERT(this, TaggedIsNotSmi(object_prototype));
    var_object_map.Bind(LoadMap(object_prototype));
    Goto(&loop);
  }

  BIND(&return_true);
  var_result.Bind(TrueConstant());
  Goto(&return_result);

  BIND(&return_false);
  var_result.Bind(FalseConstant());
  Goto(&return_result);

  BIND(&return_runtime);
  {
    // Fallback to the runtime implementation.
    var_result.Bind(
        CallRuntime(Runtime::kHasInPrototypeChain, context, object, prototype));
  }
  Goto(&return_result);

  BIND(&return_result);
  return var_result.value();
}

Node* CodeStubAssembler::OrdinaryHasInstance(Node* context, Node* callable,
                                             Node* object) {
  VARIABLE(var_result, MachineRepresentation::kTagged);
  Label return_runtime(this, Label::kDeferred), return_result(this);

  GotoIfForceSlowPath(&return_runtime);

  // Goto runtime if {object} is a Smi.
  GotoIf(TaggedIsSmi(object), &return_runtime);

  // Goto runtime if {callable} is a Smi.
  GotoIf(TaggedIsSmi(callable), &return_runtime);

  // Load map of {callable}.
  TNode<Map> callable_map = LoadMap(callable);

  // Goto runtime if {callable} is not a JSFunction.
  TNode<Uint16T> callable_instance_type = LoadMapInstanceType(callable_map);
  GotoIfNot(InstanceTypeEqual(callable_instance_type, JS_FUNCTION_TYPE),
            &return_runtime);

  GotoIfPrototypeRequiresRuntimeLookup(CAST(callable), callable_map,
                                       &return_runtime);

  // Get the "prototype" (or initial map) of the {callable}.
  TNode<HeapObject> callable_prototype = LoadObjectField<HeapObject>(
      CAST(callable), JSFunction::kPrototypeOrInitialMapOffset);
  {
    Label no_initial_map(this), walk_prototype_chain(this);
    TVARIABLE(HeapObject, var_callable_prototype, callable_prototype);

    // Resolve the "prototype" if the {callable} has an initial map.
    GotoIfNot(IsMap(callable_prototype), &no_initial_map);
    var_callable_prototype =
        LoadObjectField<HeapObject>(callable_prototype, Map::kPrototypeOffset);
    Goto(&walk_prototype_chain);

    BIND(&no_initial_map);
    // {callable_prototype} is the hole if the "prototype" property hasn't been
    // requested so far.
    Branch(TaggedEqual(callable_prototype, TheHoleConstant()), &return_runtime,
           &walk_prototype_chain);

    BIND(&walk_prototype_chain);
    callable_prototype = var_callable_prototype.value();
  }

  // Loop through the prototype chain looking for the {callable} prototype.
  CSA_ASSERT(this, IsJSReceiver(callable_prototype));
  var_result.Bind(HasInPrototypeChain(context, object, callable_prototype));
  Goto(&return_result);

  BIND(&return_runtime);
  {
    // Fallback to the runtime implementation.
    var_result.Bind(
        CallRuntime(Runtime::kOrdinaryHasInstance, context, callable, object));
  }
  Goto(&return_result);

  BIND(&return_result);
  return var_result.value();
}

TNode<IntPtrT> CodeStubAssembler::ElementOffsetFromIndex(Node* index_node,
                                                         ElementsKind kind,
                                                         ParameterMode mode,
                                                         int base_size) {
  CSA_SLOW_ASSERT(this, MatchesParameterMode(index_node, mode));
  int element_size_shift = ElementsKindToShiftSize(kind);
  int element_size = 1 << element_size_shift;
  int const kSmiShiftBits = kSmiShiftSize + kSmiTagSize;
  intptr_t index = 0;
  bool constant_index = false;
  if (mode == SMI_PARAMETERS) {
    element_size_shift -= kSmiShiftBits;
    Smi smi_index;
    constant_index = ToSmiConstant(index_node, &smi_index);
    if (constant_index) index = smi_index.value();
    index_node = BitcastTaggedSignedToWord(index_node);
  } else {
    DCHECK(mode == INTPTR_PARAMETERS);
    constant_index = ToIntPtrConstant(index_node, &index);
  }
  if (constant_index) {
    return IntPtrConstant(base_size + element_size * index);
  }

  TNode<WordT> shifted_index =
      (element_size_shift == 0)
          ? UncheckedCast<WordT>(index_node)
          : ((element_size_shift > 0)
                 ? WordShl(index_node, IntPtrConstant(element_size_shift))
                 : WordSar(index_node, IntPtrConstant(-element_size_shift)));
  return IntPtrAdd(IntPtrConstant(base_size), Signed(shifted_index));
}

TNode<BoolT> CodeStubAssembler::IsOffsetInBounds(SloppyTNode<IntPtrT> offset,
                                                 SloppyTNode<IntPtrT> length,
                                                 int header_size,
                                                 ElementsKind kind) {
  // Make sure we point to the last field.
  int element_size = 1 << ElementsKindToShiftSize(kind);
  int correction = header_size - kHeapObjectTag - element_size;
  TNode<IntPtrT> last_offset =
      ElementOffsetFromIndex(length, kind, INTPTR_PARAMETERS, correction);
  return IntPtrLessThanOrEqual(offset, last_offset);
}

TNode<HeapObject> CodeStubAssembler::LoadFeedbackCellValue(
    SloppyTNode<JSFunction> closure) {
  TNode<FeedbackCell> feedback_cell =
      LoadObjectField<FeedbackCell>(closure, JSFunction::kFeedbackCellOffset);
  return LoadObjectField<HeapObject>(feedback_cell, FeedbackCell::kValueOffset);
}

TNode<HeapObject> CodeStubAssembler::LoadFeedbackVector(
    SloppyTNode<JSFunction> closure) {
  TVARIABLE(HeapObject, maybe_vector, LoadFeedbackCellValue(closure));
  Label done(this);

  // If the closure doesn't have a feedback vector allocated yet, return
  // undefined. FeedbackCell can contain Undefined / FixedArray (for lazy
  // allocations) / FeedbackVector.
  GotoIf(IsFeedbackVector(maybe_vector.value()), &done);

  // In all other cases return Undefined.
  maybe_vector = UndefinedConstant();
  Goto(&done);

  BIND(&done);
  return maybe_vector.value();
}

TNode<ClosureFeedbackCellArray> CodeStubAssembler::LoadClosureFeedbackArray(
    SloppyTNode<JSFunction> closure) {
  TVARIABLE(HeapObject, feedback_cell_array, LoadFeedbackCellValue(closure));
  Label end(this);

  // When feedback vectors are not yet allocated feedback cell contains a
  // an array of feedback cells used by create closures.
  GotoIf(HasInstanceType(feedback_cell_array.value(),
                         CLOSURE_FEEDBACK_CELL_ARRAY_TYPE),
         &end);

  // Load FeedbackCellArray from feedback vector.
  TNode<FeedbackVector> vector = CAST(feedback_cell_array.value());
  feedback_cell_array = CAST(
      LoadObjectField(vector, FeedbackVector::kClosureFeedbackCellArrayOffset));
  Goto(&end);

  BIND(&end);
  return CAST(feedback_cell_array.value());
}

TNode<FeedbackVector> CodeStubAssembler::LoadFeedbackVectorForStub() {
  TNode<JSFunction> function =
      CAST(LoadFromParentFrame(JavaScriptFrameConstants::kFunctionOffset));
  return CAST(LoadFeedbackVector(function));
}

void CodeStubAssembler::UpdateFeedback(Node* feedback, Node* maybe_vector,
                                       Node* slot_id) {
  Label end(this);
  // If feedback_vector is not valid, then nothing to do.
  GotoIf(IsUndefined(maybe_vector), &end);

  // This method is used for binary op and compare feedback. These
  // vector nodes are initialized with a smi 0, so we can simply OR
  // our new feedback in place.
  TNode<FeedbackVector> feedback_vector = CAST(maybe_vector);
  TNode<MaybeObject> feedback_element =
      LoadFeedbackVectorSlot(feedback_vector, slot_id);
  TNode<Smi> previous_feedback = CAST(feedback_element);
  TNode<Smi> combined_feedback = SmiOr(previous_feedback, CAST(feedback));

  GotoIf(SmiEqual(previous_feedback, combined_feedback), &end);
  {
    StoreFeedbackVectorSlot(feedback_vector, slot_id, combined_feedback,
                            SKIP_WRITE_BARRIER);
    ReportFeedbackUpdate(feedback_vector, slot_id, "UpdateFeedback");
    Goto(&end);
  }

  BIND(&end);
}

void CodeStubAssembler::ReportFeedbackUpdate(
    SloppyTNode<FeedbackVector> feedback_vector, SloppyTNode<IntPtrT> slot_id,
    const char* reason) {
  // Reset profiler ticks.
  StoreObjectFieldNoWriteBarrier(
      feedback_vector, FeedbackVector::kProfilerTicksOffset, Int32Constant(0),
      MachineRepresentation::kWord32);

#ifdef V8_TRACE_FEEDBACK_UPDATES
  // Trace the update.
  CallRuntime(Runtime::kInterpreterTraceUpdateFeedback, NoContextConstant(),
              LoadFromParentFrame(JavaScriptFrameConstants::kFunctionOffset),
              SmiTag(slot_id), StringConstant(reason));
#endif  // V8_TRACE_FEEDBACK_UPDATES
}

void CodeStubAssembler::OverwriteFeedback(Variable* existing_feedback,
                                          int new_feedback) {
  if (existing_feedback == nullptr) return;
  existing_feedback->Bind(SmiConstant(new_feedback));
}

void CodeStubAssembler::CombineFeedback(Variable* existing_feedback,
                                        int feedback) {
  if (existing_feedback == nullptr) return;
  existing_feedback->Bind(
      SmiOr(CAST(existing_feedback->value()), SmiConstant(feedback)));
}

void CodeStubAssembler::CombineFeedback(Variable* existing_feedback,
                                        Node* feedback) {
  if (existing_feedback == nullptr) return;
  existing_feedback->Bind(
      SmiOr(CAST(existing_feedback->value()), CAST(feedback)));
}

void CodeStubAssembler::CheckForAssociatedProtector(SloppyTNode<Name> name,
                                                    Label* if_protector) {
  // This list must be kept in sync with LookupIterator::UpdateProtector!
  // TODO(jkummerow): Would it be faster to have a bit in Symbol::flags()?
  GotoIf(TaggedEqual(name, ConstructorStringConstant()), if_protector);
  GotoIf(TaggedEqual(name, IteratorSymbolConstant()), if_protector);
  GotoIf(TaggedEqual(name, NextStringConstant()), if_protector);
  GotoIf(TaggedEqual(name, SpeciesSymbolConstant()), if_protector);
  GotoIf(TaggedEqual(name, IsConcatSpreadableSymbolConstant()), if_protector);
  GotoIf(TaggedEqual(name, ResolveStringConstant()), if_protector);
  GotoIf(TaggedEqual(name, ThenStringConstant()), if_protector);
  // Fall through if no case matched.
}

TNode<Map> CodeStubAssembler::LoadReceiverMap(SloppyTNode<Object> receiver) {
  return Select<Map>(
      TaggedIsSmi(receiver), [=] { return HeapNumberMapConstant(); },
      [=] { return LoadMap(UncheckedCast<HeapObject>(receiver)); });
}

TNode<IntPtrT> CodeStubAssembler::TryToIntptr(Node* key, Label* miss) {
  TVARIABLE(IntPtrT, var_intptr_key);
  Label done(this, &var_intptr_key), key_is_smi(this);
  GotoIf(TaggedIsSmi(key), &key_is_smi);
  // Try to convert a heap number to a Smi.
  GotoIfNot(IsHeapNumber(key), miss);
  {
    TNode<Float64T> value = LoadHeapNumberValue(key);
    TNode<Int32T> int_value = RoundFloat64ToInt32(value);
    GotoIfNot(Float64Equal(value, ChangeInt32ToFloat64(int_value)), miss);
    var_intptr_key = ChangeInt32ToIntPtr(int_value);
    Goto(&done);
  }

  BIND(&key_is_smi);
  {
    var_intptr_key = SmiUntag(key);
    Goto(&done);
  }

  BIND(&done);
  return var_intptr_key.value();
}

Node* CodeStubAssembler::EmitKeyedSloppyArguments(
    Node* receiver, Node* key, Node* value, Label* bailout,
    ArgumentsAccessMode access_mode) {
  // Mapped arguments are actual arguments. Unmapped arguments are values added
  // to the arguments object after it was created for the call. Mapped arguments
  // are stored in the context at indexes given by elements[key + 2]. Unmapped
  // arguments are stored as regular indexed properties in the arguments array,
  // held at elements[1]. See NewSloppyArguments() in runtime.cc for a detailed
  // look at argument object construction.
  //
  // The sloppy arguments elements array has a special format:
  //
  // 0: context
  // 1: unmapped arguments array
  // 2: mapped_index0,
  // 3: mapped_index1,
  // ...
  //
  // length is 2 + min(number_of_actual_arguments, number_of_formal_arguments).
  // If key + 2 >= elements.length then attempt to look in the unmapped
  // arguments array (given by elements[1]) and return the value at key, missing
  // to the runtime if the unmapped arguments array is not a fixed array or if
  // key >= unmapped_arguments_array.length.
  //
  // Otherwise, t = elements[key + 2]. If t is the hole, then look up the value
  // in the unmapped arguments array, as described above. Otherwise, t is a Smi
  // index into the context array given at elements[0]. Return the value at
  // context[t].

  GotoIfNot(TaggedIsSmi(key), bailout);
  key = SmiUntag(key);
  GotoIf(IntPtrLessThan(key, IntPtrConstant(0)), bailout);

  TNode<FixedArray> elements = CAST(LoadElements(receiver));
  TNode<IntPtrT> elements_length = LoadAndUntagFixedArrayBaseLength(elements);

  VARIABLE(var_result, MachineRepresentation::kTagged);
  if (access_mode == ArgumentsAccessMode::kStore) {
    var_result.Bind(value);
  } else {
    DCHECK(access_mode == ArgumentsAccessMode::kLoad ||
           access_mode == ArgumentsAccessMode::kHas);
  }
  Label if_mapped(this), if_unmapped(this), end(this, &var_result);
  TNode<IntPtrT> intptr_two = IntPtrConstant(2);
  TNode<WordT> adjusted_length = IntPtrSub(elements_length, intptr_two);

  GotoIf(UintPtrGreaterThanOrEqual(key, adjusted_length), &if_unmapped);

  TNode<Object> mapped_index =
      LoadFixedArrayElement(elements, IntPtrAdd(key, intptr_two));
  Branch(TaggedEqual(mapped_index, TheHoleConstant()), &if_unmapped,
         &if_mapped);

  BIND(&if_mapped);
  {
    TNode<IntPtrT> mapped_index_intptr = SmiUntag(CAST(mapped_index));
    TNode<Context> the_context = CAST(LoadFixedArrayElement(elements, 0));
    if (access_mode == ArgumentsAccessMode::kLoad) {
      TNode<Object> result =
          LoadContextElement(the_context, mapped_index_intptr);
      CSA_ASSERT(this, TaggedNotEqual(result, TheHoleConstant()));
      var_result.Bind(result);
    } else if (access_mode == ArgumentsAccessMode::kHas) {
      CSA_ASSERT(this, Word32BinaryNot(IsTheHole(LoadContextElement(
                           the_context, mapped_index_intptr))));
      var_result.Bind(TrueConstant());
    } else {
      StoreContextElement(the_context, mapped_index_intptr, value);
    }
    Goto(&end);
  }

  BIND(&if_unmapped);
  {
    TNode<HeapObject> backing_store_ho =
        CAST(LoadFixedArrayElement(elements, 1));
    GotoIf(TaggedNotEqual(LoadMap(backing_store_ho), FixedArrayMapConstant()),
           bailout);
    TNode<FixedArray> backing_store = CAST(backing_store_ho);

    TNode<IntPtrT> backing_store_length =
        LoadAndUntagFixedArrayBaseLength(backing_store);
    if (access_mode == ArgumentsAccessMode::kHas) {
      Label out_of_bounds(this);
      GotoIf(UintPtrGreaterThanOrEqual(key, backing_store_length),
             &out_of_bounds);
      TNode<Object> result = LoadFixedArrayElement(backing_store, key);
      var_result.Bind(
          SelectBooleanConstant(TaggedNotEqual(result, TheHoleConstant())));
      Goto(&end);

      BIND(&out_of_bounds);
      var_result.Bind(FalseConstant());
      Goto(&end);
    } else {
      GotoIf(UintPtrGreaterThanOrEqual(key, backing_store_length), bailout);

      // The key falls into unmapped range.
      if (access_mode == ArgumentsAccessMode::kLoad) {
        TNode<Object> result = LoadFixedArrayElement(backing_store, key);
        GotoIf(TaggedEqual(result, TheHoleConstant()), bailout);
        var_result.Bind(result);
      } else {
        StoreFixedArrayElement(backing_store, key, value);
      }
      Goto(&end);
    }
  }

  BIND(&end);
  return var_result.value();
}

TNode<Context> CodeStubAssembler::LoadScriptContext(
    TNode<Context> context, TNode<IntPtrT> context_index) {
  TNode<NativeContext> native_context = LoadNativeContext(context);
  TNode<ScriptContextTable> script_context_table = CAST(
      LoadContextElement(native_context, Context::SCRIPT_CONTEXT_TABLE_INDEX));

  TNode<Context> script_context = CAST(LoadFixedArrayElement(
      script_context_table, context_index,
      ScriptContextTable::kFirstContextSlotIndex * kTaggedSize));
  return script_context;
}

namespace {

// Converts typed array elements kind to a machine representations.
MachineRepresentation ElementsKindToMachineRepresentation(ElementsKind kind) {
  switch (kind) {
    case UINT8_CLAMPED_ELEMENTS:
    case UINT8_ELEMENTS:
    case INT8_ELEMENTS:
      return MachineRepresentation::kWord8;
    case UINT16_ELEMENTS:
    case INT16_ELEMENTS:
      return MachineRepresentation::kWord16;
    case UINT32_ELEMENTS:
    case INT32_ELEMENTS:
      return MachineRepresentation::kWord32;
    case FLOAT32_ELEMENTS:
      return MachineRepresentation::kFloat32;
    case FLOAT64_ELEMENTS:
      return MachineRepresentation::kFloat64;
    default:
      UNREACHABLE();
  }
}

}  // namespace

void CodeStubAssembler::StoreElement(Node* elements, ElementsKind kind,
                                     Node* index, Node* value,
                                     ParameterMode mode) {
  if (kind == BIGINT64_ELEMENTS || kind == BIGUINT64_ELEMENTS) {
    TNode<IntPtrT> offset = ElementOffsetFromIndex(index, kind, mode, 0);
    TVARIABLE(UintPtrT, var_low);
    // Only used on 32-bit platforms.
    TVARIABLE(UintPtrT, var_high);
    BigIntToRawBytes(CAST(value), &var_low, &var_high);

    MachineRepresentation rep = WordT::kMachineRepresentation;
#if defined(V8_TARGET_BIG_ENDIAN)
    if (!Is64()) {
      StoreNoWriteBarrier(rep, elements, offset, var_high.value());
      StoreNoWriteBarrier(rep, elements,
                          IntPtrAdd(offset, IntPtrConstant(kSystemPointerSize)),
                          var_low.value());
    } else {
      StoreNoWriteBarrier(rep, elements, offset, var_low.value());
    }
#else
    StoreNoWriteBarrier(rep, elements, offset, var_low.value());
    if (!Is64()) {
      StoreNoWriteBarrier(rep, elements,
                          IntPtrAdd(offset, IntPtrConstant(kSystemPointerSize)),
                          var_high.value());
    }
#endif
  } else if (IsTypedArrayElementsKind(kind)) {
    if (kind == UINT8_CLAMPED_ELEMENTS) {
      CSA_ASSERT(this, Word32Equal(UncheckedCast<Word32T>(value),
                                   Word32And(Int32Constant(0xFF), value)));
    }
    TNode<IntPtrT> offset = ElementOffsetFromIndex(index, kind, mode, 0);
    // TODO(cbruni): Add OOB check once typed.
    MachineRepresentation rep = ElementsKindToMachineRepresentation(kind);
    StoreNoWriteBarrier(rep, elements, offset, value);
    return;
  } else if (IsDoubleElementsKind(kind)) {
    TNode<Float64T> value_float64 = UncheckedCast<Float64T>(value);
    StoreFixedDoubleArrayElement(CAST(elements), index, value_float64, mode);
  } else {
    WriteBarrierMode barrier_mode = IsSmiElementsKind(kind)
                                        ? UNSAFE_SKIP_WRITE_BARRIER
                                        : UPDATE_WRITE_BARRIER;
    StoreFixedArrayElement(CAST(elements), index, value, barrier_mode, 0, mode);
  }
}

Node* CodeStubAssembler::Int32ToUint8Clamped(Node* int32_value) {
  Label done(this);
  TNode<Int32T> int32_zero = Int32Constant(0);
  TNode<Int32T> int32_255 = Int32Constant(255);
  VARIABLE(var_value, MachineRepresentation::kWord32, int32_value);
  GotoIf(Uint32LessThanOrEqual(int32_value, int32_255), &done);
  var_value.Bind(int32_zero);
  GotoIf(Int32LessThan(int32_value, int32_zero), &done);
  var_value.Bind(int32_255);
  Goto(&done);
  BIND(&done);
  return var_value.value();
}

Node* CodeStubAssembler::Float64ToUint8Clamped(Node* float64_value) {
  Label done(this);
  VARIABLE(var_value, MachineRepresentation::kWord32, Int32Constant(0));
  GotoIf(Float64LessThanOrEqual(float64_value, Float64Constant(0.0)), &done);
  var_value.Bind(Int32Constant(255));
  GotoIf(Float64LessThanOrEqual(Float64Constant(255.0), float64_value), &done);
  {
    TNode<Float64T> rounded_value = Float64RoundToEven(float64_value);
    var_value.Bind(TruncateFloat64ToWord32(rounded_value));
    Goto(&done);
  }
  BIND(&done);
  return var_value.value();
}

Node* CodeStubAssembler::PrepareValueForWriteToTypedArray(
    TNode<Object> input, ElementsKind elements_kind, TNode<Context> context) {
  DCHECK(IsTypedArrayElementsKind(elements_kind));

  MachineRepresentation rep;
  switch (elements_kind) {
    case UINT8_ELEMENTS:
    case INT8_ELEMENTS:
    case UINT16_ELEMENTS:
    case INT16_ELEMENTS:
    case UINT32_ELEMENTS:
    case INT32_ELEMENTS:
    case UINT8_CLAMPED_ELEMENTS:
      rep = MachineRepresentation::kWord32;
      break;
    case FLOAT32_ELEMENTS:
      rep = MachineRepresentation::kFloat32;
      break;
    case FLOAT64_ELEMENTS:
      rep = MachineRepresentation::kFloat64;
      break;
    case BIGINT64_ELEMENTS:
    case BIGUINT64_ELEMENTS:
      return ToBigInt(context, input);
    default:
      UNREACHABLE();
  }

  VARIABLE(var_result, rep);
  VARIABLE(var_input, MachineRepresentation::kTagged, input);
  Label done(this, &var_result), if_smi(this), if_heapnumber_or_oddball(this),
      convert(this), loop(this, &var_input);
  Goto(&loop);
  BIND(&loop);
  GotoIf(TaggedIsSmi(var_input.value()), &if_smi);
  // We can handle both HeapNumber and Oddball here, since Oddball has the
  // same layout as the HeapNumber for the HeapNumber::value field. This
  // way we can also properly optimize stores of oddballs to typed arrays.
  GotoIf(IsHeapNumber(var_input.value()), &if_heapnumber_or_oddball);
  STATIC_ASSERT_FIELD_OFFSETS_EQUAL(HeapNumber::kValueOffset,
                                    Oddball::kToNumberRawOffset);
  Branch(HasInstanceType(var_input.value(), ODDBALL_TYPE),
         &if_heapnumber_or_oddball, &convert);

  BIND(&if_heapnumber_or_oddball);
  {
    TNode<Float64T> value = UncheckedCast<Float64T>(LoadObjectField(
        var_input.value(), HeapNumber::kValueOffset, MachineType::Float64()));
    if (rep == MachineRepresentation::kWord32) {
      if (elements_kind == UINT8_CLAMPED_ELEMENTS) {
        var_result.Bind(Float64ToUint8Clamped(value));
      } else {
        var_result.Bind(TruncateFloat64ToWord32(value));
      }
    } else if (rep == MachineRepresentation::kFloat32) {
      var_result.Bind(TruncateFloat64ToFloat32(value));
    } else {
      DCHECK_EQ(MachineRepresentation::kFloat64, rep);
      var_result.Bind(value);
    }
    Goto(&done);
  }

  BIND(&if_smi);
  {
    TNode<Int32T> value = SmiToInt32(var_input.value());
    if (rep == MachineRepresentation::kFloat32) {
      var_result.Bind(RoundInt32ToFloat32(value));
    } else if (rep == MachineRepresentation::kFloat64) {
      var_result.Bind(ChangeInt32ToFloat64(value));
    } else {
      DCHECK_EQ(MachineRepresentation::kWord32, rep);
      if (elements_kind == UINT8_CLAMPED_ELEMENTS) {
        var_result.Bind(Int32ToUint8Clamped(value));
      } else {
        var_result.Bind(value);
      }
    }
    Goto(&done);
  }

  BIND(&convert);
  {
    var_input.Bind(CallBuiltin(Builtins::kNonNumberToNumber, context, input));
    Goto(&loop);
  }

  BIND(&done);
  return var_result.value();
}

void CodeStubAssembler::BigIntToRawBytes(TNode<BigInt> bigint,
                                         TVariable<UintPtrT>* var_low,
                                         TVariable<UintPtrT>* var_high) {
  Label done(this);
  *var_low = Unsigned(IntPtrConstant(0));
  *var_high = Unsigned(IntPtrConstant(0));
  TNode<Word32T> bitfield = LoadBigIntBitfield(bigint);
  TNode<Uint32T> length = DecodeWord32<BigIntBase::LengthBits>(bitfield);
  TNode<Uint32T> sign = DecodeWord32<BigIntBase::SignBits>(bitfield);
  GotoIf(Word32Equal(length, Int32Constant(0)), &done);
  *var_low = LoadBigIntDigit(bigint, 0);
  if (!Is64()) {
    Label load_done(this);
    GotoIf(Word32Equal(length, Int32Constant(1)), &load_done);
    *var_high = LoadBigIntDigit(bigint, 1);
    Goto(&load_done);
    BIND(&load_done);
  }
  GotoIf(Word32Equal(sign, Int32Constant(0)), &done);
  // Negative value. Simulate two's complement.
  if (!Is64()) {
    *var_high = Unsigned(IntPtrSub(IntPtrConstant(0), var_high->value()));
    Label no_carry(this);
    GotoIf(IntPtrEqual(var_low->value(), IntPtrConstant(0)), &no_carry);
    *var_high = Unsigned(IntPtrSub(var_high->value(), IntPtrConstant(1)));
    Goto(&no_carry);
    BIND(&no_carry);
  }
  *var_low = Unsigned(IntPtrSub(IntPtrConstant(0), var_low->value()));
  Goto(&done);
  BIND(&done);
}

void CodeStubAssembler::EmitElementStore(Node* object, Node* key, Node* value,
                                         ElementsKind elements_kind,
                                         KeyedAccessStoreMode store_mode,
                                         Label* bailout, Node* context,
                                         Variable* maybe_converted_value) {
  CSA_ASSERT(this, Word32BinaryNot(IsJSProxy(object)));

  TNode<FixedArrayBase> elements = LoadElements(object);
  if (!(IsSmiOrObjectElementsKind(elements_kind) ||
        IsSealedElementsKind(elements_kind) ||
        IsNonextensibleElementsKind(elements_kind))) {
    CSA_ASSERT(this, Word32BinaryNot(IsFixedCOWArrayMap(LoadMap(elements))));
  } else if (!IsCOWHandlingStoreMode(store_mode)) {
    GotoIf(IsFixedCOWArrayMap(LoadMap(elements)), bailout);
  }

  // TODO(ishell): introduce TryToIntPtrOrSmi() and use OptimalParameterMode().
  ParameterMode parameter_mode = INTPTR_PARAMETERS;
  TNode<IntPtrT> intptr_key = TryToIntptr(key, bailout);

  if (IsTypedArrayElementsKind(elements_kind)) {
    Label done(this), update_value_and_bailout(this, Label::kDeferred);

    // IntegerIndexedElementSet converts value to a Number/BigInt prior to the
    // bounds check.
    Node* converted_value = PrepareValueForWriteToTypedArray(
        CAST(value), elements_kind, CAST(context));

    // There must be no allocations between the buffer load and
    // and the actual store to backing store, because GC may decide that
    // the buffer is not alive or move the elements.
    // TODO(ishell): introduce DisallowHeapAllocationCode scope here.

    // Check if buffer has been detached.
    TNode<JSArrayBuffer> buffer = LoadJSArrayBufferViewBuffer(CAST(object));
    if (maybe_converted_value) {
      GotoIf(IsDetachedBuffer(buffer), &update_value_and_bailout);
    } else {
      GotoIf(IsDetachedBuffer(buffer), bailout);
    }

    // Bounds check.
    TNode<UintPtrT> length = LoadJSTypedArrayLength(CAST(object));

    if (store_mode == STORE_IGNORE_OUT_OF_BOUNDS) {
      // Skip the store if we write beyond the length or
      // to a property with a negative integer index.
      GotoIfNot(UintPtrLessThan(intptr_key, length), &done);
    } else {
      DCHECK_EQ(store_mode, STANDARD_STORE);
      GotoIfNot(UintPtrLessThan(intptr_key, length), &update_value_and_bailout);
    }

    TNode<RawPtrT> backing_store = LoadJSTypedArrayBackingStore(CAST(object));
    StoreElement(backing_store, elements_kind, intptr_key, converted_value,
                 parameter_mode);
    Goto(&done);

    BIND(&update_value_and_bailout);
    // We already prepared the incoming value for storing into a typed array.
    // This might involve calling ToNumber in some cases. We shouldn't call
    // ToNumber again in the runtime so pass the converted value to the runtime.
    // The prepared value is an untagged value. Convert it to a tagged value
    // to pass it to runtime. It is not possible to do the detached buffer check
    // before we prepare the value, since ToNumber can detach the ArrayBuffer.
    // The spec specifies the order of these operations.
    if (maybe_converted_value != nullptr) {
      switch (elements_kind) {
        case UINT8_ELEMENTS:
        case INT8_ELEMENTS:
        case UINT16_ELEMENTS:
        case INT16_ELEMENTS:
        case UINT8_CLAMPED_ELEMENTS:
          maybe_converted_value->Bind(SmiFromInt32(converted_value));
          break;
        case UINT32_ELEMENTS:
          maybe_converted_value->Bind(ChangeUint32ToTagged(converted_value));
          break;
        case INT32_ELEMENTS:
          maybe_converted_value->Bind(ChangeInt32ToTagged(converted_value));
          break;
        case FLOAT32_ELEMENTS: {
          Label dont_allocate_heap_number(this), end(this);
          GotoIf(TaggedIsSmi(value), &dont_allocate_heap_number);
          GotoIf(IsHeapNumber(value), &dont_allocate_heap_number);
          {
            maybe_converted_value->Bind(AllocateHeapNumberWithValue(
                ChangeFloat32ToFloat64(converted_value)));
            Goto(&end);
          }
          BIND(&dont_allocate_heap_number);
          {
            maybe_converted_value->Bind(value);
            Goto(&end);
          }
          BIND(&end);
          break;
        }
        case FLOAT64_ELEMENTS: {
          Label dont_allocate_heap_number(this), end(this);
          GotoIf(TaggedIsSmi(value), &dont_allocate_heap_number);
          GotoIf(IsHeapNumber(value), &dont_allocate_heap_number);
          {
            maybe_converted_value->Bind(
                AllocateHeapNumberWithValue(converted_value));
            Goto(&end);
          }
          BIND(&dont_allocate_heap_number);
          {
            maybe_converted_value->Bind(value);
            Goto(&end);
          }
          BIND(&end);
          break;
        }
        case BIGINT64_ELEMENTS:
        case BIGUINT64_ELEMENTS:
          maybe_converted_value->Bind(converted_value);
          break;
        default:
          UNREACHABLE();
      }
    }
    Goto(bailout);

    BIND(&done);
    return;
  }
  DCHECK(IsFastElementsKind(elements_kind) ||
         IsSealedElementsKind(elements_kind) ||
         IsNonextensibleElementsKind(elements_kind));

  Node* length = SelectImpl(
      IsJSArray(object), [=]() { return LoadJSArrayLength(object); },
      [=]() { return LoadFixedArrayBaseLength(elements); },
      MachineRepresentation::kTagged);
  length = TaggedToParameter(length, parameter_mode);

  // In case value is stored into a fast smi array, assure that the value is
  // a smi before manipulating the backing store. Otherwise the backing store
  // may be left in an invalid state.
  if (IsSmiElementsKind(elements_kind)) {
    GotoIfNot(TaggedIsSmi(value), bailout);
  } else if (IsDoubleElementsKind(elements_kind)) {
    value = TryTaggedToFloat64(value, bailout);
  }

  if (IsGrowStoreMode(store_mode) &&
      !(IsSealedElementsKind(elements_kind) ||
        IsNonextensibleElementsKind(elements_kind))) {
    elements =
        CAST(CheckForCapacityGrow(object, elements, elements_kind, length,
                                  intptr_key, parameter_mode, bailout));
  } else {
    GotoIfNot(UintPtrLessThan(intptr_key, length), bailout);
  }

  // Cannot store to a hole in holey sealed elements so bailout.
  if (elements_kind == HOLEY_SEALED_ELEMENTS ||
      elements_kind == HOLEY_NONEXTENSIBLE_ELEMENTS) {
    TNode<Object> target_value =
        LoadFixedArrayElement(CAST(elements), intptr_key);
    GotoIf(IsTheHole(target_value), bailout);
  }

  // If we didn't grow {elements}, it might still be COW, in which case we
  // copy it now.
  if (!(IsSmiOrObjectElementsKind(elements_kind) ||
        IsSealedElementsKind(elements_kind) ||
        IsNonextensibleElementsKind(elements_kind))) {
    CSA_ASSERT(this, Word32BinaryNot(IsFixedCOWArrayMap(LoadMap(elements))));
  } else if (IsCOWHandlingStoreMode(store_mode)) {
    elements = CAST(CopyElementsOnWrite(object, elements, elements_kind, length,
                                        parameter_mode, bailout));
  }

  CSA_ASSERT(this, Word32BinaryNot(IsFixedCOWArrayMap(LoadMap(elements))));
  StoreElement(elements, elements_kind, intptr_key, value, parameter_mode);
}

Node* CodeStubAssembler::CheckForCapacityGrow(Node* object, Node* elements,
                                              ElementsKind kind,
                                              SloppyTNode<UintPtrT> length,
                                              SloppyTNode<WordT> key,
                                              ParameterMode mode,
                                              Label* bailout) {
  DCHECK(IsFastElementsKind(kind));
  VARIABLE(checked_elements, MachineRepresentation::kTagged);
  Label grow_case(this), no_grow_case(this), done(this),
      grow_bailout(this, Label::kDeferred);

  Node* condition;
  if (IsHoleyElementsKind(kind)) {
    condition = UintPtrGreaterThanOrEqual(key, length);
  } else {
    // We don't support growing here unless the value is being appended.
    condition = WordEqual(key, length);
  }
  Branch(condition, &grow_case, &no_grow_case);

  BIND(&grow_case);
  {
    Node* current_capacity =
        TaggedToParameter(LoadFixedArrayBaseLength(elements), mode);
    checked_elements.Bind(elements);
    Label fits_capacity(this);
    // If key is negative, we will notice in Runtime::kGrowArrayElements.
    GotoIf(UintPtrLessThan(key, current_capacity), &fits_capacity);

    {
      Node* new_elements = TryGrowElementsCapacity(
          object, elements, kind, key, current_capacity, mode, &grow_bailout);
      checked_elements.Bind(new_elements);
      Goto(&fits_capacity);
    }

    BIND(&grow_bailout);
    {
      Node* tagged_key = mode == SMI_PARAMETERS
                             ? static_cast<Node*>(key)
                             : ChangeInt32ToTagged(TruncateWordToInt32(key));
      TNode<Object> maybe_elements = CallRuntime(
          Runtime::kGrowArrayElements, NoContextConstant(), object, tagged_key);
      GotoIf(TaggedIsSmi(maybe_elements), bailout);
      CSA_ASSERT(this, IsFixedArrayWithKind(CAST(maybe_elements), kind));
      checked_elements.Bind(maybe_elements);
      Goto(&fits_capacity);
    }

    BIND(&fits_capacity);
    GotoIfNot(IsJSArray(object), &done);

    TNode<WordT> new_length = IntPtrAdd(key, IntPtrOrSmiConstant(1, mode));
    StoreObjectFieldNoWriteBarrier(object, JSArray::kLengthOffset,
                                   ParameterToTagged(new_length, mode));
    Goto(&done);
  }

  BIND(&no_grow_case);
  {
    GotoIfNot(UintPtrLessThan(key, length), bailout);
    checked_elements.Bind(elements);
    Goto(&done);
  }

  BIND(&done);
  return checked_elements.value();
}

Node* CodeStubAssembler::CopyElementsOnWrite(Node* object, Node* elements,
                                             ElementsKind kind, Node* length,
                                             ParameterMode mode,
                                             Label* bailout) {
  VARIABLE(new_elements_var, MachineRepresentation::kTagged, elements);
  Label done(this);

  GotoIfNot(IsFixedCOWArrayMap(LoadMap(elements)), &done);
  {
    Node* capacity =
        TaggedToParameter(LoadFixedArrayBaseLength(elements), mode);
    Node* new_elements = GrowElementsCapacity(object, elements, kind, kind,
                                              length, capacity, mode, bailout);
    new_elements_var.Bind(new_elements);
    Goto(&done);
  }

  BIND(&done);
  return new_elements_var.value();
}

void CodeStubAssembler::TransitionElementsKind(Node* object, Node* map,
                                               ElementsKind from_kind,
                                               ElementsKind to_kind,
                                               Label* bailout) {
  DCHECK(!IsHoleyElementsKind(from_kind) || IsHoleyElementsKind(to_kind));
  if (AllocationSite::ShouldTrack(from_kind, to_kind)) {
    TrapAllocationMemento(object, bailout);
  }

  if (!IsSimpleMapChangeTransition(from_kind, to_kind)) {
    Comment("Non-simple map transition");
    TNode<FixedArrayBase> elements = LoadElements(object);

    Label done(this);
    GotoIf(TaggedEqual(elements, EmptyFixedArrayConstant()), &done);

    // TODO(ishell): Use OptimalParameterMode().
    ParameterMode mode = INTPTR_PARAMETERS;
    TNode<IntPtrT> elements_length =
        SmiUntag(LoadFixedArrayBaseLength(elements));
    Node* array_length = SelectImpl(
        IsJSArray(object),
        [=]() {
          CSA_ASSERT(this, IsFastElementsKind(LoadElementsKind(object)));
          return SmiUntag(LoadFastJSArrayLength(object));
        },
        [=]() { return elements_length; },
        MachineType::PointerRepresentation());

    CSA_ASSERT(this, WordNotEqual(elements_length, IntPtrConstant(0)));

    GrowElementsCapacity(object, elements, from_kind, to_kind, array_length,
                         elements_length, mode, bailout);
    Goto(&done);
    BIND(&done);
  }

  StoreMap(object, map);
}

void CodeStubAssembler::TrapAllocationMemento(Node* object,
                                              Label* memento_found) {
  Comment("[ TrapAllocationMemento");
  Label no_memento_found(this);
  Label top_check(this), map_check(this);

  TNode<ExternalReference> new_space_top_address = ExternalConstant(
      ExternalReference::new_space_allocation_top_address(isolate()));
  const int kMementoMapOffset = JSArray::kSize;
  const int kMementoLastWordOffset =
      kMementoMapOffset + AllocationMemento::kSize - kTaggedSize;

  // Bail out if the object is not in new space.
  TNode<IntPtrT> object_word = BitcastTaggedToWord(object);
  TNode<IntPtrT> object_page = PageFromAddress(object_word);
  {
    TNode<IntPtrT> page_flags =
        UncheckedCast<IntPtrT>(Load(MachineType::IntPtr(), object_page,
                                    IntPtrConstant(Page::kFlagsOffset)));
    GotoIf(WordEqual(
               WordAnd(page_flags,
                       IntPtrConstant(MemoryChunk::kIsInYoungGenerationMask)),
               IntPtrConstant(0)),
           &no_memento_found);
    // TODO(ulan): Support allocation memento for a large object by allocating
    // additional word for the memento after the large object.
    GotoIf(WordNotEqual(WordAnd(page_flags,
                                IntPtrConstant(MemoryChunk::kIsLargePageMask)),
                        IntPtrConstant(0)),
           &no_memento_found);
  }

  TNode<IntPtrT> memento_last_word = IntPtrAdd(
      object_word, IntPtrConstant(kMementoLastWordOffset - kHeapObjectTag));
  TNode<IntPtrT> memento_last_word_page = PageFromAddress(memento_last_word);

  TNode<IntPtrT> new_space_top = UncheckedCast<IntPtrT>(
      Load(MachineType::Pointer(), new_space_top_address));
  TNode<IntPtrT> new_space_top_page = PageFromAddress(new_space_top);

  // If the object is in new space, we need to check whether respective
  // potential memento object is on the same page as the current top.
  GotoIf(WordEqual(memento_last_word_page, new_space_top_page), &top_check);

  // The object is on a different page than allocation top. Bail out if the
  // object sits on the page boundary as no memento can follow and we cannot
  // touch the memory following it.
  Branch(WordEqual(object_page, memento_last_word_page), &map_check,
         &no_memento_found);

  // If top is on the same page as the current object, we need to check whether
  // we are below top.
  BIND(&top_check);
  {
    Branch(UintPtrGreaterThanOrEqual(memento_last_word, new_space_top),
           &no_memento_found, &map_check);
  }

  // Memento map check.
  BIND(&map_check);
  {
    TNode<Object> memento_map = LoadObjectField(object, kMementoMapOffset);
    Branch(TaggedEqual(memento_map, AllocationMementoMapConstant()),
           memento_found, &no_memento_found);
  }
  BIND(&no_memento_found);
  Comment("] TrapAllocationMemento");
}

TNode<IntPtrT> CodeStubAssembler::PageFromAddress(TNode<IntPtrT> address) {
  return WordAnd(address, IntPtrConstant(~kPageAlignmentMask));
}

TNode<AllocationSite> CodeStubAssembler::CreateAllocationSiteInFeedbackVector(
    SloppyTNode<FeedbackVector> feedback_vector, TNode<Smi> slot) {
  TNode<IntPtrT> size = IntPtrConstant(AllocationSite::kSizeWithWeakNext);
  TNode<HeapObject> site = Allocate(size, CodeStubAssembler::kPretenured);
  StoreMapNoWriteBarrier(site, RootIndex::kAllocationSiteWithWeakNextMap);
  // Should match AllocationSite::Initialize.
  TNode<WordT> field = UpdateWord<AllocationSite::ElementsKindBits>(
      IntPtrConstant(0), IntPtrConstant(GetInitialFastElementsKind()));
  StoreObjectFieldNoWriteBarrier(
      site, AllocationSite::kTransitionInfoOrBoilerplateOffset,
      SmiTag(Signed(field)));

  // Unlike literals, constructed arrays don't have nested sites
  TNode<Smi> zero = SmiConstant(0);
  StoreObjectFieldNoWriteBarrier(site, AllocationSite::kNestedSiteOffset, zero);

  // Pretenuring calculation field.
  StoreObjectFieldNoWriteBarrier(site, AllocationSite::kPretenureDataOffset,
                                 Int32Constant(0),
                                 MachineRepresentation::kWord32);

  // Pretenuring memento creation count field.
  StoreObjectFieldNoWriteBarrier(
      site, AllocationSite::kPretenureCreateCountOffset, Int32Constant(0),
      MachineRepresentation::kWord32);

  // Store an empty fixed array for the code dependency.
  StoreObjectFieldRoot(site, AllocationSite::kDependentCodeOffset,
                       RootIndex::kEmptyWeakFixedArray);

  // Link the object to the allocation site list
  TNode<ExternalReference> site_list = ExternalConstant(
      ExternalReference::allocation_sites_list_address(isolate()));
  TNode<Object> next_site =
      LoadBufferObject(ReinterpretCast<RawPtrT>(site_list), 0);

  // TODO(mvstanton): This is a store to a weak pointer, which we may want to
  // mark as such in order to skip the write barrier, once we have a unified
  // system for weakness. For now we decided to keep it like this because having
  // an initial write barrier backed store makes this pointer strong until the
  // next GC, and allocation sites are designed to survive several GCs anyway.
  StoreObjectField(site, AllocationSite::kWeakNextOffset, next_site);
  StoreFullTaggedNoWriteBarrier(site_list, site);

  StoreFeedbackVectorSlot(feedback_vector, slot, site, UPDATE_WRITE_BARRIER, 0,
                          SMI_PARAMETERS);
  return CAST(site);
}

TNode<MaybeObject> CodeStubAssembler::StoreWeakReferenceInFeedbackVector(
    SloppyTNode<FeedbackVector> feedback_vector, Node* slot,
    SloppyTNode<HeapObject> value, int additional_offset,
    ParameterMode parameter_mode) {
  TNode<MaybeObject> weak_value = MakeWeak(value);
  StoreFeedbackVectorSlot(feedback_vector, slot, weak_value,
                          UPDATE_WRITE_BARRIER, additional_offset,
                          parameter_mode);
  return weak_value;
}

TNode<BoolT> CodeStubAssembler::NotHasBoilerplate(
    TNode<Object> maybe_literal_site) {
  return TaggedIsSmi(maybe_literal_site);
}

TNode<Smi> CodeStubAssembler::LoadTransitionInfo(
    TNode<AllocationSite> allocation_site) {
  TNode<Smi> transition_info = CAST(LoadObjectField(
      allocation_site, AllocationSite::kTransitionInfoOrBoilerplateOffset));
  return transition_info;
}

TNode<JSObject> CodeStubAssembler::LoadBoilerplate(
    TNode<AllocationSite> allocation_site) {
  TNode<JSObject> boilerplate = CAST(LoadObjectField(
      allocation_site, AllocationSite::kTransitionInfoOrBoilerplateOffset));
  return boilerplate;
}

TNode<Int32T> CodeStubAssembler::LoadElementsKind(
    TNode<AllocationSite> allocation_site) {
  TNode<Smi> transition_info = LoadTransitionInfo(allocation_site);
  TNode<Int32T> elements_kind =
      Signed(DecodeWord32<AllocationSite::ElementsKindBits>(
          SmiToInt32(transition_info)));
  CSA_ASSERT(this, IsFastElementsKind(elements_kind));
  return elements_kind;
}

Node* CodeStubAssembler::BuildFastLoop(
    const CodeStubAssembler::VariableList& vars, Node* start_index,
    Node* end_index, const FastLoopBody& body, int increment,
    ParameterMode parameter_mode, IndexAdvanceMode advance_mode) {
  CSA_SLOW_ASSERT(this, MatchesParameterMode(start_index, parameter_mode));
  CSA_SLOW_ASSERT(this, MatchesParameterMode(end_index, parameter_mode));
  MachineRepresentation index_rep = ParameterRepresentation(parameter_mode);
  VARIABLE(var, index_rep, start_index);
  VariableList vars_copy(vars.begin(), vars.end(), zone());
  vars_copy.push_back(&var);
  Label loop(this, vars_copy);
  Label after_loop(this);
  // Introduce an explicit second check of the termination condition before the
  // loop that helps turbofan generate better code. If there's only a single
  // check, then the CodeStubAssembler forces it to be at the beginning of the
  // loop requiring a backwards branch at the end of the loop (it's not possible
  // to force the loop header check at the end of the loop and branch forward to
  // it from the pre-header). The extra branch is slower in the case that the
  // loop actually iterates.
  TNode<BoolT> first_check =
      IntPtrOrSmiEqual(var.value(), end_index, parameter_mode);
  int32_t first_check_val;
  if (ToInt32Constant(first_check, &first_check_val)) {
    if (first_check_val) return var.value();
    Goto(&loop);
  } else {
    Branch(first_check, &after_loop, &loop);
  }

  BIND(&loop);
  {
    if (advance_mode == IndexAdvanceMode::kPre) {
      Increment(&var, increment, parameter_mode);
    }
    body(var.value());
    if (advance_mode == IndexAdvanceMode::kPost) {
      Increment(&var, increment, parameter_mode);
    }
    Branch(IntPtrOrSmiNotEqual(var.value(), end_index, parameter_mode), &loop,
           &after_loop);
  }
  BIND(&after_loop);
  return var.value();
}

void CodeStubAssembler::BuildFastFixedArrayForEach(
    const CodeStubAssembler::VariableList& vars, Node* fixed_array,
    ElementsKind kind, Node* first_element_inclusive,
    Node* last_element_exclusive, const FastFixedArrayForEachBody& body,
    ParameterMode mode, ForEachDirection direction) {
  STATIC_ASSERT(FixedArray::kHeaderSize == FixedDoubleArray::kHeaderSize);
  CSA_SLOW_ASSERT(this, MatchesParameterMode(first_element_inclusive, mode));
  CSA_SLOW_ASSERT(this, MatchesParameterMode(last_element_exclusive, mode));
  CSA_SLOW_ASSERT(this, Word32Or(IsFixedArrayWithKind(fixed_array, kind),
                                 IsPropertyArray(fixed_array)));
  int32_t first_val;
  bool constant_first = ToInt32Constant(first_element_inclusive, &first_val);
  int32_t last_val;
  bool constent_last = ToInt32Constant(last_element_exclusive, &last_val);
  if (constant_first && constent_last) {
    int delta = last_val - first_val;
    DCHECK_GE(delta, 0);
    if (delta <= kElementLoopUnrollThreshold) {
      if (direction == ForEachDirection::kForward) {
        for (int i = first_val; i < last_val; ++i) {
          TNode<IntPtrT> index = IntPtrConstant(i);
          TNode<IntPtrT> offset =
              ElementOffsetFromIndex(index, kind, INTPTR_PARAMETERS,
                                     FixedArray::kHeaderSize - kHeapObjectTag);
          body(fixed_array, offset);
        }
      } else {
        for (int i = last_val - 1; i >= first_val; --i) {
          TNode<IntPtrT> index = IntPtrConstant(i);
          TNode<IntPtrT> offset =
              ElementOffsetFromIndex(index, kind, INTPTR_PARAMETERS,
                                     FixedArray::kHeaderSize - kHeapObjectTag);
          body(fixed_array, offset);
        }
      }
      return;
    }
  }

  TNode<IntPtrT> start =
      ElementOffsetFromIndex(first_element_inclusive, kind, mode,
                             FixedArray::kHeaderSize - kHeapObjectTag);
  TNode<IntPtrT> limit =
      ElementOffsetFromIndex(last_element_exclusive, kind, mode,
                             FixedArray::kHeaderSize - kHeapObjectTag);
  if (direction == ForEachDirection::kReverse) std::swap(start, limit);

  int increment = IsDoubleElementsKind(kind) ? kDoubleSize : kTaggedSize;
  BuildFastLoop(
      vars, start, limit,
      [fixed_array, &body](Node* offset) { body(fixed_array, offset); },
      direction == ForEachDirection::kReverse ? -increment : increment,
      INTPTR_PARAMETERS,
      direction == ForEachDirection::kReverse ? IndexAdvanceMode::kPre
                                              : IndexAdvanceMode::kPost);
}

void CodeStubAssembler::GotoIfFixedArraySizeDoesntFitInNewSpace(
    Node* element_count, Label* doesnt_fit, int base_size, ParameterMode mode) {
  GotoIf(FixedArraySizeDoesntFitInNewSpace(element_count, base_size, mode),
         doesnt_fit);
}

void CodeStubAssembler::InitializeFieldsWithRoot(Node* object,
                                                 Node* start_offset,
                                                 Node* end_offset,
                                                 RootIndex root_index) {
  CSA_SLOW_ASSERT(this, TaggedIsNotSmi(object));
  start_offset = IntPtrAdd(start_offset, IntPtrConstant(-kHeapObjectTag));
  end_offset = IntPtrAdd(end_offset, IntPtrConstant(-kHeapObjectTag));
  TNode<Object> root_value = LoadRoot(root_index);
  BuildFastLoop(
      end_offset, start_offset,
      [this, object, root_value](Node* current) {
        StoreNoWriteBarrier(MachineRepresentation::kTagged, object, current,
                            root_value);
      },
      -kTaggedSize, INTPTR_PARAMETERS,
      CodeStubAssembler::IndexAdvanceMode::kPre);
}

void CodeStubAssembler::BranchIfNumberRelationalComparison(
    Operation op, SloppyTNode<Number> left, SloppyTNode<Number> right,
    Label* if_true, Label* if_false) {
  CSA_SLOW_ASSERT(this, IsNumber(left));
  CSA_SLOW_ASSERT(this, IsNumber(right));

  Label do_float_comparison(this);
  TVARIABLE(Float64T, var_left_float);
  TVARIABLE(Float64T, var_right_float);

  Branch(
      TaggedIsSmi(left),
      [&] {
        TNode<Smi> smi_left = CAST(left);

        Branch(
            TaggedIsSmi(right),
            [&] {
              TNode<Smi> smi_right = CAST(right);

              // Both {left} and {right} are Smi, so just perform a fast
              // Smi comparison.
              switch (op) {
                case Operation::kEqual:
                  BranchIfSmiEqual(smi_left, smi_right, if_true, if_false);
                  break;
                case Operation::kLessThan:
                  BranchIfSmiLessThan(smi_left, smi_right, if_true, if_false);
                  break;
                case Operation::kLessThanOrEqual:
                  BranchIfSmiLessThanOrEqual(smi_left, smi_right, if_true,
                                             if_false);
                  break;
                case Operation::kGreaterThan:
                  BranchIfSmiLessThan(smi_right, smi_left, if_true, if_false);
                  break;
                case Operation::kGreaterThanOrEqual:
                  BranchIfSmiLessThanOrEqual(smi_right, smi_left, if_true,
                                             if_false);
                  break;
                default:
                  UNREACHABLE();
              }
            },
            [&] {
              var_left_float = SmiToFloat64(smi_left);
              var_right_float = LoadHeapNumberValue(CAST(right));
              Goto(&do_float_comparison);
            });
      },
      [&] {
        var_left_float = LoadHeapNumberValue(CAST(left));

        Branch(
            TaggedIsSmi(right),
            [&] {
              var_right_float = SmiToFloat64(CAST(right));
              Goto(&do_float_comparison);
            },
            [&] {
              var_right_float = LoadHeapNumberValue(CAST(right));
              Goto(&do_float_comparison);
            });
      });

  BIND(&do_float_comparison);
  {
    switch (op) {
      case Operation::kEqual:
        Branch(Float64Equal(var_left_float.value(), var_right_float.value()),
               if_true, if_false);
        break;
      case Operation::kLessThan:
        Branch(Float64LessThan(var_left_float.value(), var_right_float.value()),
               if_true, if_false);
        break;
      case Operation::kLessThanOrEqual:
        Branch(Float64LessThanOrEqual(var_left_float.value(),
                                      var_right_float.value()),
               if_true, if_false);
        break;
      case Operation::kGreaterThan:
        Branch(
            Float64GreaterThan(var_left_float.value(), var_right_float.value()),
            if_true, if_false);
        break;
      case Operation::kGreaterThanOrEqual:
        Branch(Float64GreaterThanOrEqual(var_left_float.value(),
                                         var_right_float.value()),
               if_true, if_false);
        break;
      default:
        UNREACHABLE();
    }
  }
}

void CodeStubAssembler::GotoIfNumberGreaterThanOrEqual(Node* left, Node* right,
                                                       Label* if_true) {
  Label if_false(this);
  BranchIfNumberRelationalComparison(Operation::kGreaterThanOrEqual, left,
                                     right, if_true, &if_false);
  BIND(&if_false);
}

namespace {
Operation Reverse(Operation op) {
  switch (op) {
    case Operation::kLessThan:
      return Operation::kGreaterThan;
    case Operation::kLessThanOrEqual:
      return Operation::kGreaterThanOrEqual;
    case Operation::kGreaterThan:
      return Operation::kLessThan;
    case Operation::kGreaterThanOrEqual:
      return Operation::kLessThanOrEqual;
    default:
      break;
  }
  UNREACHABLE();
}
}  // anonymous namespace

Node* CodeStubAssembler::RelationalComparison(Operation op,
                                              SloppyTNode<Object> left,
                                              SloppyTNode<Object> right,
                                              SloppyTNode<Context> context,
                                              Variable* var_type_feedback) {
  Label return_true(this), return_false(this), do_float_comparison(this),
      end(this);
  TVARIABLE(Oddball, var_result);  // Actually only "true" or "false".
  TVARIABLE(Float64T, var_left_float);
  TVARIABLE(Float64T, var_right_float);

  // We might need to loop several times due to ToPrimitive and/or ToNumeric
  // conversions.
  TVARIABLE(Object, var_left, left);
  TVARIABLE(Object, var_right, right);
  VariableList loop_variable_list({&var_left, &var_right}, zone());
  if (var_type_feedback != nullptr) {
    // Initialize the type feedback to None. The current feedback is combined
    // with the previous feedback.
    var_type_feedback->Bind(SmiConstant(CompareOperationFeedback::kNone));
    loop_variable_list.push_back(var_type_feedback);
  }
  Label loop(this, loop_variable_list);
  Goto(&loop);
  BIND(&loop);
  {
    left = var_left.value();
    right = var_right.value();

    Label if_left_smi(this), if_left_not_smi(this);
    Branch(TaggedIsSmi(left), &if_left_smi, &if_left_not_smi);

    BIND(&if_left_smi);
    {
      TNode<Smi> smi_left = CAST(left);
      Label if_right_smi(this), if_right_heapnumber(this),
          if_right_bigint(this, Label::kDeferred),
          if_right_not_numeric(this, Label::kDeferred);
      GotoIf(TaggedIsSmi(right), &if_right_smi);
      TNode<Map> right_map = LoadMap(CAST(right));
      GotoIf(IsHeapNumberMap(right_map), &if_right_heapnumber);
      TNode<Uint16T> right_instance_type = LoadMapInstanceType(right_map);
      Branch(IsBigIntInstanceType(right_instance_type), &if_right_bigint,
             &if_right_not_numeric);

      BIND(&if_right_smi);
      {
        TNode<Smi> smi_right = CAST(right);
        CombineFeedback(var_type_feedback,
                        CompareOperationFeedback::kSignedSmall);
        switch (op) {
          case Operation::kLessThan:
            BranchIfSmiLessThan(smi_left, smi_right, &return_true,
                                &return_false);
            break;
          case Operation::kLessThanOrEqual:
            BranchIfSmiLessThanOrEqual(smi_left, smi_right, &return_true,
                                       &return_false);
            break;
          case Operation::kGreaterThan:
            BranchIfSmiLessThan(smi_right, smi_left, &return_true,
                                &return_false);
            break;
          case Operation::kGreaterThanOrEqual:
            BranchIfSmiLessThanOrEqual(smi_right, smi_left, &return_true,
                                       &return_false);
            break;
          default:
            UNREACHABLE();
        }
      }

      BIND(&if_right_heapnumber);
      {
        CombineFeedback(var_type_feedback, CompareOperationFeedback::kNumber);
        var_left_float = SmiToFloat64(smi_left);
        var_right_float = LoadHeapNumberValue(CAST(right));
        Goto(&do_float_comparison);
      }

      BIND(&if_right_bigint);
      {
        OverwriteFeedback(var_type_feedback, CompareOperationFeedback::kAny);
        var_result = CAST(CallRuntime(Runtime::kBigIntCompareToNumber,
                                      NoContextConstant(),
                                      SmiConstant(Reverse(op)), right, left));
        Goto(&end);
      }

      BIND(&if_right_not_numeric);
      {
        OverwriteFeedback(var_type_feedback, CompareOperationFeedback::kAny);
        // Convert {right} to a Numeric; we don't need to perform the
        // dedicated ToPrimitive(right, hint Number) operation, as the
        // ToNumeric(right) will by itself already invoke ToPrimitive with
        // a Number hint.
        var_right = CallBuiltin(Builtins::kNonNumberToNumeric, context, right);
        Goto(&loop);
      }
    }

    BIND(&if_left_not_smi);
    {
      TNode<Map> left_map = LoadMap(CAST(left));

      Label if_right_smi(this), if_right_not_smi(this);
      Branch(TaggedIsSmi(right), &if_right_smi, &if_right_not_smi);

      BIND(&if_right_smi);
      {
        Label if_left_heapnumber(this), if_left_bigint(this, Label::kDeferred),
            if_left_not_numeric(this, Label::kDeferred);
        GotoIf(IsHeapNumberMap(left_map), &if_left_heapnumber);
        TNode<Uint16T> left_instance_type = LoadMapInstanceType(left_map);
        Branch(IsBigIntInstanceType(left_instance_type), &if_left_bigint,
               &if_left_not_numeric);

        BIND(&if_left_heapnumber);
        {
          CombineFeedback(var_type_feedback, CompareOperationFeedback::kNumber);
          var_left_float = LoadHeapNumberValue(CAST(left));
          var_right_float = SmiToFloat64(CAST(right));
          Goto(&do_float_comparison);
        }

        BIND(&if_left_bigint);
        {
          OverwriteFeedback(var_type_feedback, CompareOperationFeedback::kAny);
          var_result = CAST(CallRuntime(Runtime::kBigIntCompareToNumber,
                                        NoContextConstant(), SmiConstant(op),
                                        left, right));
          Goto(&end);
        }

        BIND(&if_left_not_numeric);
        {
          OverwriteFeedback(var_type_feedback, CompareOperationFeedback::kAny);
          // Convert {left} to a Numeric; we don't need to perform the
          // dedicated ToPrimitive(left, hint Number) operation, as the
          // ToNumeric(left) will by itself already invoke ToPrimitive with
          // a Number hint.
          var_left = CallBuiltin(Builtins::kNonNumberToNumeric, context, left);
          Goto(&loop);
        }
      }

      BIND(&if_right_not_smi);
      {
        TNode<Map> right_map = LoadMap(CAST(right));

        Label if_left_heapnumber(this), if_left_bigint(this, Label::kDeferred),
            if_left_string(this, Label::kDeferred),
            if_left_other(this, Label::kDeferred);
        GotoIf(IsHeapNumberMap(left_map), &if_left_heapnumber);
        TNode<Uint16T> left_instance_type = LoadMapInstanceType(left_map);
        GotoIf(IsBigIntInstanceType(left_instance_type), &if_left_bigint);
        Branch(IsStringInstanceType(left_instance_type), &if_left_string,
               &if_left_other);

        BIND(&if_left_heapnumber);
        {
          Label if_right_heapnumber(this),
              if_right_bigint(this, Label::kDeferred),
              if_right_not_numeric(this, Label::kDeferred);
          GotoIf(TaggedEqual(right_map, left_map), &if_right_heapnumber);
          TNode<Uint16T> right_instance_type = LoadMapInstanceType(right_map);
          Branch(IsBigIntInstanceType(right_instance_type), &if_right_bigint,
                 &if_right_not_numeric);

          BIND(&if_right_heapnumber);
          {
            CombineFeedback(var_type_feedback,
                            CompareOperationFeedback::kNumber);
            var_left_float = LoadHeapNumberValue(CAST(left));
            var_right_float = LoadHeapNumberValue(CAST(right));
            Goto(&do_float_comparison);
          }

          BIND(&if_right_bigint);
          {
            OverwriteFeedback(var_type_feedback,
                              CompareOperationFeedback::kAny);
            var_result = CAST(CallRuntime(
                Runtime::kBigIntCompareToNumber, NoContextConstant(),
                SmiConstant(Reverse(op)), right, left));
            Goto(&end);
          }

          BIND(&if_right_not_numeric);
          {
            OverwriteFeedback(var_type_feedback,
                              CompareOperationFeedback::kAny);
            // Convert {right} to a Numeric; we don't need to perform
            // dedicated ToPrimitive(right, hint Number) operation, as the
            // ToNumeric(right) will by itself already invoke ToPrimitive with
            // a Number hint.
            var_right =
                CallBuiltin(Builtins::kNonNumberToNumeric, context, right);
            Goto(&loop);
          }
        }

        BIND(&if_left_bigint);
        {
          Label if_right_heapnumber(this), if_right_bigint(this),
              if_right_string(this), if_right_other(this);
          GotoIf(IsHeapNumberMap(right_map), &if_right_heapnumber);
          TNode<Uint16T> right_instance_type = LoadMapInstanceType(right_map);
          GotoIf(IsBigIntInstanceType(right_instance_type), &if_right_bigint);
          Branch(IsStringInstanceType(right_instance_type), &if_right_string,
                 &if_right_other);

          BIND(&if_right_heapnumber);
          {
            OverwriteFeedback(var_type_feedback,
                              CompareOperationFeedback::kAny);
            var_result = CAST(CallRuntime(Runtime::kBigIntCompareToNumber,
                                          NoContextConstant(), SmiConstant(op),
                                          left, right));
            Goto(&end);
          }

          BIND(&if_right_bigint);
          {
            CombineFeedback(var_type_feedback,
                            CompareOperationFeedback::kBigInt);
            var_result = CAST(CallRuntime(Runtime::kBigIntCompareToBigInt,
                                          NoContextConstant(), SmiConstant(op),
                                          left, right));
            Goto(&end);
          }

          BIND(&if_right_string);
          {
            OverwriteFeedback(var_type_feedback,
                              CompareOperationFeedback::kAny);
            var_result = CAST(CallRuntime(Runtime::kBigIntCompareToString,
                                          NoContextConstant(), SmiConstant(op),
                                          left, right));
            Goto(&end);
          }

          // {right} is not a Number, BigInt, or String.
          BIND(&if_right_other);
          {
            OverwriteFeedback(var_type_feedback,
                              CompareOperationFeedback::kAny);
            // Convert {right} to a Numeric; we don't need to perform
            // dedicated ToPrimitive(right, hint Number) operation, as the
            // ToNumeric(right) will by itself already invoke ToPrimitive with
            // a Number hint.
            var_right =
                CallBuiltin(Builtins::kNonNumberToNumeric, context, right);
            Goto(&loop);
          }
        }

        BIND(&if_left_string);
        {
          TNode<Uint16T> right_instance_type = LoadMapInstanceType(right_map);

          Label if_right_not_string(this, Label::kDeferred);
          GotoIfNot(IsStringInstanceType(right_instance_type),
                    &if_right_not_string);

          // Both {left} and {right} are strings.
          CombineFeedback(var_type_feedback, CompareOperationFeedback::kString);
          Builtins::Name builtin;
          switch (op) {
            case Operation::kLessThan:
              builtin = Builtins::kStringLessThan;
              break;
            case Operation::kLessThanOrEqual:
              builtin = Builtins::kStringLessThanOrEqual;
              break;
            case Operation::kGreaterThan:
              builtin = Builtins::kStringGreaterThan;
              break;
            case Operation::kGreaterThanOrEqual:
              builtin = Builtins::kStringGreaterThanOrEqual;
              break;
            default:
              UNREACHABLE();
          }
          var_result = CAST(CallBuiltin(builtin, context, left, right));
          Goto(&end);

          BIND(&if_right_not_string);
          {
            OverwriteFeedback(var_type_feedback,
                              CompareOperationFeedback::kAny);
            // {left} is a String, while {right} isn't. Check if {right} is
            // a BigInt, otherwise call ToPrimitive(right, hint Number) if
            // {right} is a receiver, or ToNumeric(left) and then
            // ToNumeric(right) in the other cases.
            STATIC_ASSERT(LAST_JS_RECEIVER_TYPE == LAST_TYPE);
            Label if_right_bigint(this),
                if_right_receiver(this, Label::kDeferred);
            GotoIf(IsBigIntInstanceType(right_instance_type), &if_right_bigint);
            GotoIf(IsJSReceiverInstanceType(right_instance_type),
                   &if_right_receiver);

            var_left =
                CallBuiltin(Builtins::kNonNumberToNumeric, context, left);
            var_right = CallBuiltin(Builtins::kToNumeric, context, right);
            Goto(&loop);

            BIND(&if_right_bigint);
            {
              var_result = CAST(CallRuntime(
                  Runtime::kBigIntCompareToString, NoContextConstant(),
                  SmiConstant(Reverse(op)), right, left));
              Goto(&end);
            }

            BIND(&if_right_receiver);
            {
              Callable callable = CodeFactory::NonPrimitiveToPrimitive(
                  isolate(), ToPrimitiveHint::kNumber);
              var_right = CallStub(callable, context, right);
              Goto(&loop);
            }
          }
        }

        BIND(&if_left_other);
        {
          // {left} is neither a Numeric nor a String, and {right} is not a Smi.
          if (var_type_feedback != nullptr) {
            // Collect NumberOrOddball feedback if {left} is an Oddball
            // and {right} is either a HeapNumber or Oddball. Otherwise collect
            // Any feedback.
            Label collect_any_feedback(this), collect_oddball_feedback(this),
                collect_feedback_done(this);
            GotoIfNot(InstanceTypeEqual(left_instance_type, ODDBALL_TYPE),
                      &collect_any_feedback);

            GotoIf(IsHeapNumberMap(right_map), &collect_oddball_feedback);
            TNode<Uint16T> right_instance_type = LoadMapInstanceType(right_map);
            Branch(InstanceTypeEqual(right_instance_type, ODDBALL_TYPE),
                   &collect_oddball_feedback, &collect_any_feedback);

            BIND(&collect_oddball_feedback);
            {
              CombineFeedback(var_type_feedback,
                              CompareOperationFeedback::kNumberOrOddball);
              Goto(&collect_feedback_done);
            }

            BIND(&collect_any_feedback);
            {
              OverwriteFeedback(var_type_feedback,
                                CompareOperationFeedback::kAny);
              Goto(&collect_feedback_done);
            }

            BIND(&collect_feedback_done);
          }

          // If {left} is a receiver, call ToPrimitive(left, hint Number).
          // Otherwise call ToNumeric(right) and then ToNumeric(left), the
          // order here is important as it's observable by user code.
          STATIC_ASSERT(LAST_JS_RECEIVER_TYPE == LAST_TYPE);
          Label if_left_receiver(this, Label::kDeferred);
          GotoIf(IsJSReceiverInstanceType(left_instance_type),
                 &if_left_receiver);

          var_right = CallBuiltin(Builtins::kToNumeric, context, right);
          var_left = CallBuiltin(Builtins::kNonNumberToNumeric, context, left);
          Goto(&loop);

          BIND(&if_left_receiver);
          {
            Callable callable = CodeFactory::NonPrimitiveToPrimitive(
                isolate(), ToPrimitiveHint::kNumber);
            var_left = CallStub(callable, context, left);
            Goto(&loop);
          }
        }
      }
    }
  }

  BIND(&do_float_comparison);
  {
    switch (op) {
      case Operation::kLessThan:
        Branch(Float64LessThan(var_left_float.value(), var_right_float.value()),
               &return_true, &return_false);
        break;
      case Operation::kLessThanOrEqual:
        Branch(Float64LessThanOrEqual(var_left_float.value(),
                                      var_right_float.value()),
               &return_true, &return_false);
        break;
      case Operation::kGreaterThan:
        Branch(
            Float64GreaterThan(var_left_float.value(), var_right_float.value()),
            &return_true, &return_false);
        break;
      case Operation::kGreaterThanOrEqual:
        Branch(Float64GreaterThanOrEqual(var_left_float.value(),
                                         var_right_float.value()),
               &return_true, &return_false);
        break;
      default:
        UNREACHABLE();
    }
  }

  BIND(&return_true);
  {
    var_result = TrueConstant();
    Goto(&end);
  }

  BIND(&return_false);
  {
    var_result = FalseConstant();
    Goto(&end);
  }

  BIND(&end);
  return var_result.value();
}

TNode<Smi> CodeStubAssembler::CollectFeedbackForString(
    SloppyTNode<Int32T> instance_type) {
  TNode<Smi> feedback = SelectSmiConstant(
      Word32Equal(
          Word32And(instance_type, Int32Constant(kIsNotInternalizedMask)),
          Int32Constant(kInternalizedTag)),
      CompareOperationFeedback::kInternalizedString,
      CompareOperationFeedback::kString);
  return feedback;
}

void CodeStubAssembler::GenerateEqual_Same(SloppyTNode<Object> value,
                                           Label* if_equal, Label* if_notequal,
                                           Variable* var_type_feedback) {
  // In case of abstract or strict equality checks, we need additional checks
  // for NaN values because they are not considered equal, even if both the
  // left and the right hand side reference exactly the same value.

  Label if_smi(this), if_heapnumber(this);
  GotoIf(TaggedIsSmi(value), &if_smi);

  TNode<HeapObject> value_heapobject = CAST(value);
  TNode<Map> value_map = LoadMap(value_heapobject);
  GotoIf(IsHeapNumberMap(value_map), &if_heapnumber);

  // For non-HeapNumbers, all we do is collect type feedback.
  if (var_type_feedback != nullptr) {
    TNode<Uint16T> instance_type = LoadMapInstanceType(value_map);

    Label if_string(this), if_receiver(this), if_oddball(this), if_symbol(this),
        if_bigint(this);
    GotoIf(IsStringInstanceType(instance_type), &if_string);
    GotoIf(IsJSReceiverInstanceType(instance_type), &if_receiver);
    GotoIf(IsOddballInstanceType(instance_type), &if_oddball);
    Branch(IsBigIntInstanceType(instance_type), &if_bigint, &if_symbol);

    BIND(&if_string);
    {
      CSA_ASSERT(this, IsString(value_heapobject));
      CombineFeedback(var_type_feedback,
                      CollectFeedbackForString(instance_type));
      Goto(if_equal);
    }

    BIND(&if_symbol);
    {
      CSA_ASSERT(this, IsSymbol(value_heapobject));
      CombineFeedback(var_type_feedback, CompareOperationFeedback::kSymbol);
      Goto(if_equal);
    }

    BIND(&if_receiver);
    {
      CSA_ASSERT(this, IsJSReceiver(value_heapobject));
      CombineFeedback(var_type_feedback, CompareOperationFeedback::kReceiver);
      Goto(if_equal);
    }

    BIND(&if_bigint);
    {
      CSA_ASSERT(this, IsBigInt(value_heapobject));
      CombineFeedback(var_type_feedback, CompareOperationFeedback::kBigInt);
      Goto(if_equal);
    }

    BIND(&if_oddball);
    {
      CSA_ASSERT(this, IsOddball(value_heapobject));
      Label if_boolean(this), if_not_boolean(this);
      Branch(IsBooleanMap(value_map), &if_boolean, &if_not_boolean);

      BIND(&if_boolean);
      {
        CombineFeedback(var_type_feedback, CompareOperationFeedback::kAny);
        Goto(if_equal);
      }

      BIND(&if_not_boolean);
      {
        CSA_ASSERT(this, IsNullOrUndefined(value_heapobject));
        CombineFeedback(var_type_feedback,
                        CompareOperationFeedback::kReceiverOrNullOrUndefined);
        Goto(if_equal);
      }
    }
  } else {
    Goto(if_equal);
  }

  BIND(&if_heapnumber);
  {
    CombineFeedback(var_type_feedback, CompareOperationFeedback::kNumber);
    TNode<Float64T> number_value = LoadHeapNumberValue(value_heapobject);
    BranchIfFloat64IsNaN(number_value, if_notequal, if_equal);
  }

  BIND(&if_smi);
  {
    CombineFeedback(var_type_feedback, CompareOperationFeedback::kSignedSmall);
    Goto(if_equal);
  }
}

// ES6 section 7.2.12 Abstract Equality Comparison
Node* CodeStubAssembler::Equal(SloppyTNode<Object> left,
                               SloppyTNode<Object> right,
                               SloppyTNode<Context> context,
                               Variable* var_type_feedback) {
  // This is a slightly optimized version of Object::Equals. Whenever you
  // change something functionality wise in here, remember to update the
  // Object::Equals method as well.

  Label if_equal(this), if_notequal(this), do_float_comparison(this),
      do_right_stringtonumber(this, Label::kDeferred), end(this);
  VARIABLE(result, MachineRepresentation::kTagged);
  TVARIABLE(Float64T, var_left_float);
  TVARIABLE(Float64T, var_right_float);

  // We can avoid code duplication by exploiting the fact that abstract equality
  // is symmetric.
  Label use_symmetry(this);

  // We might need to loop several times due to ToPrimitive and/or ToNumber
  // conversions.
  TVARIABLE(Object, var_left, left);
  TVARIABLE(Object, var_right, right);
  VariableList loop_variable_list({&var_left, &var_right}, zone());
  if (var_type_feedback != nullptr) {
    // Initialize the type feedback to None. The current feedback will be
    // combined with the previous feedback.
    OverwriteFeedback(var_type_feedback, CompareOperationFeedback::kNone);
    loop_variable_list.push_back(var_type_feedback);
  }
  Label loop(this, loop_variable_list);
  Goto(&loop);
  BIND(&loop);
  {
    left = var_left.value();
    right = var_right.value();

    Label if_notsame(this);
    GotoIf(TaggedNotEqual(left, right), &if_notsame);
    {
      // {left} and {right} reference the exact same value, yet we need special
      // treatment for HeapNumber, as NaN is not equal to NaN.
      GenerateEqual_Same(left, &if_equal, &if_notequal, var_type_feedback);
    }

    BIND(&if_notsame);
    Label if_left_smi(this), if_left_not_smi(this);
    Branch(TaggedIsSmi(left), &if_left_smi, &if_left_not_smi);

    BIND(&if_left_smi);
    {
      Label if_right_smi(this), if_right_not_smi(this);
      Branch(TaggedIsSmi(right), &if_right_smi, &if_right_not_smi);

      BIND(&if_right_smi);
      {
        // We have already checked for {left} and {right} being the same value,
        // so when we get here they must be different Smis.
        CombineFeedback(var_type_feedback,
                        CompareOperationFeedback::kSignedSmall);
        Goto(&if_notequal);
      }

      BIND(&if_right_not_smi);
      TNode<Map> right_map = LoadMap(CAST(right));
      Label if_right_heapnumber(this), if_right_boolean(this),
          if_right_bigint(this, Label::kDeferred),
          if_right_receiver(this, Label::kDeferred);
      GotoIf(IsHeapNumberMap(right_map), &if_right_heapnumber);
      // {left} is Smi and {right} is not HeapNumber or Smi.
      if (var_type_feedback != nullptr) {
        var_type_feedback->Bind(SmiConstant(CompareOperationFeedback::kAny));
      }
      GotoIf(IsBooleanMap(right_map), &if_right_boolean);
      TNode<Uint16T> right_type = LoadMapInstanceType(right_map);
      GotoIf(IsStringInstanceType(right_type), &do_right_stringtonumber);
      GotoIf(IsBigIntInstanceType(right_type), &if_right_bigint);
      Branch(IsJSReceiverInstanceType(right_type), &if_right_receiver,
             &if_notequal);

      BIND(&if_right_heapnumber);
      {
        var_left_float = SmiToFloat64(CAST(left));
        var_right_float = LoadHeapNumberValue(CAST(right));
        CombineFeedback(var_type_feedback, CompareOperationFeedback::kNumber);
        Goto(&do_float_comparison);
      }

      BIND(&if_right_boolean);
      {
        var_right = LoadObjectField(CAST(right), Oddball::kToNumberOffset);
        Goto(&loop);
      }

      BIND(&if_right_bigint);
      {
        result.Bind(CallRuntime(Runtime::kBigIntEqualToNumber,
                                NoContextConstant(), right, left));
        Goto(&end);
      }

      BIND(&if_right_receiver);
      {
        Callable callable = CodeFactory::NonPrimitiveToPrimitive(isolate());
        var_right = CallStub(callable, context, right);
        Goto(&loop);
      }
    }

    BIND(&if_left_not_smi);
    {
      GotoIf(TaggedIsSmi(right), &use_symmetry);

      Label if_left_symbol(this), if_left_number(this),
          if_left_string(this, Label::kDeferred),
          if_left_bigint(this, Label::kDeferred), if_left_oddball(this),
          if_left_receiver(this);

      TNode<Map> left_map = LoadMap(CAST(left));
      TNode<Map> right_map = LoadMap(CAST(right));
      TNode<Uint16T> left_type = LoadMapInstanceType(left_map);
      TNode<Uint16T> right_type = LoadMapInstanceType(right_map);

      GotoIf(IsStringInstanceType(left_type), &if_left_string);
      GotoIf(IsSymbolInstanceType(left_type), &if_left_symbol);
      GotoIf(IsHeapNumberInstanceType(left_type), &if_left_number);
      GotoIf(IsOddballInstanceType(left_type), &if_left_oddball);
      Branch(IsBigIntInstanceType(left_type), &if_left_bigint,
             &if_left_receiver);

      BIND(&if_left_string);
      {
        GotoIfNot(IsStringInstanceType(right_type), &use_symmetry);
        result.Bind(CallBuiltin(Builtins::kStringEqual, context, left, right));
        CombineFeedback(var_type_feedback,
                        SmiOr(CollectFeedbackForString(left_type),
                              CollectFeedbackForString(right_type)));
        Goto(&end);
      }

      BIND(&if_left_number);
      {
        Label if_right_not_number(this);
        GotoIf(Word32NotEqual(left_type, right_type), &if_right_not_number);

        var_left_float = LoadHeapNumberValue(CAST(left));
        var_right_float = LoadHeapNumberValue(CAST(right));
        CombineFeedback(var_type_feedback, CompareOperationFeedback::kNumber);
        Goto(&do_float_comparison);

        BIND(&if_right_not_number);
        {
          Label if_right_boolean(this);
          if (var_type_feedback != nullptr) {
            var_type_feedback->Bind(
                SmiConstant(CompareOperationFeedback::kAny));
          }
          GotoIf(IsStringInstanceType(right_type), &do_right_stringtonumber);
          GotoIf(IsBooleanMap(right_map), &if_right_boolean);
          GotoIf(IsBigIntInstanceType(right_type), &use_symmetry);
          Branch(IsJSReceiverInstanceType(right_type), &use_symmetry,
                 &if_notequal);

          BIND(&if_right_boolean);
          {
            var_right = LoadObjectField(CAST(right), Oddball::kToNumberOffset);
            Goto(&loop);
          }
        }
      }

      BIND(&if_left_bigint);
      {
        Label if_right_heapnumber(this), if_right_bigint(this),
            if_right_string(this), if_right_boolean(this);
        GotoIf(IsHeapNumberMap(right_map), &if_right_heapnumber);
        GotoIf(IsBigIntInstanceType(right_type), &if_right_bigint);
        GotoIf(IsStringInstanceType(right_type), &if_right_string);
        GotoIf(IsBooleanMap(right_map), &if_right_boolean);
        Branch(IsJSReceiverInstanceType(right_type), &use_symmetry,
               &if_notequal);

        BIND(&if_right_heapnumber);
        {
          if (var_type_feedback != nullptr) {
            var_type_feedback->Bind(
                SmiConstant(CompareOperationFeedback::kAny));
          }
          result.Bind(CallRuntime(Runtime::kBigIntEqualToNumber,
                                  NoContextConstant(), left, right));
          Goto(&end);
        }

        BIND(&if_right_bigint);
        {
          CombineFeedback(var_type_feedback, CompareOperationFeedback::kBigInt);
          result.Bind(CallRuntime(Runtime::kBigIntEqualToBigInt,
                                  NoContextConstant(), left, right));
          Goto(&end);
        }

        BIND(&if_right_string);
        {
          if (var_type_feedback != nullptr) {
            var_type_feedback->Bind(
                SmiConstant(CompareOperationFeedback::kAny));
          }
          result.Bind(CallRuntime(Runtime::kBigIntEqualToString,
                                  NoContextConstant(), left, right));
          Goto(&end);
        }

        BIND(&if_right_boolean);
        {
          if (var_type_feedback != nullptr) {
            var_type_feedback->Bind(
                SmiConstant(CompareOperationFeedback::kAny));
          }
          var_right = LoadObjectField(CAST(right), Oddball::kToNumberOffset);
          Goto(&loop);
        }
      }

      BIND(&if_left_oddball);
      {
        Label if_left_boolean(this), if_left_not_boolean(this);
        Branch(IsBooleanMap(left_map), &if_left_boolean, &if_left_not_boolean);

        BIND(&if_left_not_boolean);
        {
          // {left} is either Null or Undefined. Check if {right} is
          // undetectable (which includes Null and Undefined).
          Label if_right_undetectable(this), if_right_not_undetectable(this);
          Branch(IsUndetectableMap(right_map), &if_right_undetectable,
                 &if_right_not_undetectable);

          BIND(&if_right_undetectable);
          {
            if (var_type_feedback != nullptr) {
              // If {right} is undetectable, it must be either also
              // Null or Undefined, or a Receiver (aka document.all).
              var_type_feedback->Bind(SmiConstant(
                  CompareOperationFeedback::kReceiverOrNullOrUndefined));
            }
            Goto(&if_equal);
          }

          BIND(&if_right_not_undetectable);
          {
            if (var_type_feedback != nullptr) {
              // Track whether {right} is Null, Undefined or Receiver.
              var_type_feedback->Bind(SmiConstant(
                  CompareOperationFeedback::kReceiverOrNullOrUndefined));
              GotoIf(IsJSReceiverInstanceType(right_type), &if_notequal);
              GotoIfNot(IsBooleanMap(right_map), &if_notequal);
              var_type_feedback->Bind(
                  SmiConstant(CompareOperationFeedback::kAny));
            }
            Goto(&if_notequal);
          }
        }

        BIND(&if_left_boolean);
        {
          if (var_type_feedback != nullptr) {
            var_type_feedback->Bind(
                SmiConstant(CompareOperationFeedback::kAny));
          }

          // If {right} is a Boolean too, it must be a different Boolean.
          GotoIf(TaggedEqual(right_map, left_map), &if_notequal);

          // Otherwise, convert {left} to number and try again.
          var_left = LoadObjectField(CAST(left), Oddball::kToNumberOffset);
          Goto(&loop);
        }
      }

      BIND(&if_left_symbol);
      {
        Label if_right_receiver(this);
        GotoIf(IsJSReceiverInstanceType(right_type), &if_right_receiver);
        // {right} is not a JSReceiver and also not the same Symbol as {left},
        // so the result is "not equal".
        if (var_type_feedback != nullptr) {
          Label if_right_symbol(this);
          GotoIf(IsSymbolInstanceType(right_type), &if_right_symbol);
          var_type_feedback->Bind(SmiConstant(CompareOperationFeedback::kAny));
          Goto(&if_notequal);

          BIND(&if_right_symbol);
          {
            CombineFeedback(var_type_feedback,
                            CompareOperationFeedback::kSymbol);
            Goto(&if_notequal);
          }
        } else {
          Goto(&if_notequal);
        }

        BIND(&if_right_receiver);
        {
          // {left} is a Primitive and {right} is a JSReceiver, so swapping
          // the order is not observable.
          if (var_type_feedback != nullptr) {
            var_type_feedback->Bind(
                SmiConstant(CompareOperationFeedback::kAny));
          }
          Goto(&use_symmetry);
        }
      }

      BIND(&if_left_receiver);
      {
        CSA_ASSERT(this, IsJSReceiverInstanceType(left_type));
        Label if_right_receiver(this), if_right_not_receiver(this);
        Branch(IsJSReceiverInstanceType(right_type), &if_right_receiver,
               &if_right_not_receiver);

        BIND(&if_right_receiver);
        {
          // {left} and {right} are different JSReceiver references.
          CombineFeedback(var_type_feedback,
                          CompareOperationFeedback::kReceiver);
          Goto(&if_notequal);
        }

        BIND(&if_right_not_receiver);
        {
          // Check if {right} is undetectable, which means it must be Null
          // or Undefined, since we already ruled out Receiver for {right}.
          Label if_right_undetectable(this),
              if_right_not_undetectable(this, Label::kDeferred);
          Branch(IsUndetectableMap(right_map), &if_right_undetectable,
                 &if_right_not_undetectable);

          BIND(&if_right_undetectable);
          {
            // When we get here, {right} must be either Null or Undefined.
            CSA_ASSERT(this, IsNullOrUndefined(right));
            if (var_type_feedback != nullptr) {
              var_type_feedback->Bind(SmiConstant(
                  CompareOperationFeedback::kReceiverOrNullOrUndefined));
            }
            Branch(IsUndetectableMap(left_map), &if_equal, &if_notequal);
          }

          BIND(&if_right_not_undetectable);
          {
            // {right} is a Primitive, and neither Null or Undefined;
            // convert {left} to Primitive too.
            if (var_type_feedback != nullptr) {
              var_type_feedback->Bind(
                  SmiConstant(CompareOperationFeedback::kAny));
            }
            Callable callable = CodeFactory::NonPrimitiveToPrimitive(isolate());
            var_left = CallStub(callable, context, left);
            Goto(&loop);
          }
        }
      }
    }

    BIND(&do_right_stringtonumber);
    {
      var_right = CallBuiltin(Builtins::kStringToNumber, context, right);
      Goto(&loop);
    }

    BIND(&use_symmetry);
    {
      var_left = right;
      var_right = left;
      Goto(&loop);
    }
  }

  BIND(&do_float_comparison);
  {
    Branch(Float64Equal(var_left_float.value(), var_right_float.value()),
           &if_equal, &if_notequal);
  }

  BIND(&if_equal);
  {
    result.Bind(TrueConstant());
    Goto(&end);
  }

  BIND(&if_notequal);
  {
    result.Bind(FalseConstant());
    Goto(&end);
  }

  BIND(&end);
  return result.value();
}

TNode<Oddball> CodeStubAssembler::StrictEqual(SloppyTNode<Object> lhs,
                                              SloppyTNode<Object> rhs,
                                              Variable* var_type_feedback) {
  // Pseudo-code for the algorithm below:
  //
  // if (lhs == rhs) {
  //   if (lhs->IsHeapNumber()) return HeapNumber::cast(lhs)->value() != NaN;
  //   return true;
  // }
  // if (!lhs->IsSmi()) {
  //   if (lhs->IsHeapNumber()) {
  //     if (rhs->IsSmi()) {
  //       return Smi::ToInt(rhs) == HeapNumber::cast(lhs)->value();
  //     } else if (rhs->IsHeapNumber()) {
  //       return HeapNumber::cast(rhs)->value() ==
  //       HeapNumber::cast(lhs)->value();
  //     } else {
  //       return false;
  //     }
  //   } else {
  //     if (rhs->IsSmi()) {
  //       return false;
  //     } else {
  //       if (lhs->IsString()) {
  //         if (rhs->IsString()) {
  //           return %StringEqual(lhs, rhs);
  //         } else {
  //           return false;
  //         }
  //       } else if (lhs->IsBigInt()) {
  //         if (rhs->IsBigInt()) {
  //           return %BigIntEqualToBigInt(lhs, rhs);
  //         } else {
  //           return false;
  //         }
  //       } else {
  //         return false;
  //       }
  //     }
  //   }
  // } else {
  //   if (rhs->IsSmi()) {
  //     return false;
  //   } else {
  //     if (rhs->IsHeapNumber()) {
  //       return Smi::ToInt(lhs) == HeapNumber::cast(rhs)->value();
  //     } else {
  //       return false;
  //     }
  //   }
  // }

  Label if_equal(this), if_notequal(this), if_not_equivalent_types(this),
      end(this);
  TVARIABLE(Oddball, result);

  OverwriteFeedback(var_type_feedback, CompareOperationFeedback::kNone);

  // Check if {lhs} and {rhs} refer to the same object.
  Label if_same(this), if_notsame(this);
  Branch(TaggedEqual(lhs, rhs), &if_same, &if_notsame);

  BIND(&if_same);
  {
    // The {lhs} and {rhs} reference the exact same value, yet we need special
    // treatment for HeapNumber, as NaN is not equal to NaN.
    GenerateEqual_Same(lhs, &if_equal, &if_notequal, var_type_feedback);
  }

  BIND(&if_notsame);
  {
    // The {lhs} and {rhs} reference different objects, yet for Smi, HeapNumber,
    // BigInt and String they can still be considered equal.

    // Check if {lhs} is a Smi or a HeapObject.
    Label if_lhsissmi(this), if_lhsisnotsmi(this);
    Branch(TaggedIsSmi(lhs), &if_lhsissmi, &if_lhsisnotsmi);

    BIND(&if_lhsisnotsmi);
    {
      // Load the map of {lhs}.
      TNode<Map> lhs_map = LoadMap(CAST(lhs));

      // Check if {lhs} is a HeapNumber.
      Label if_lhsisnumber(this), if_lhsisnotnumber(this);
      Branch(IsHeapNumberMap(lhs_map), &if_lhsisnumber, &if_lhsisnotnumber);

      BIND(&if_lhsisnumber);
      {
        // Check if {rhs} is a Smi or a HeapObject.
        Label if_rhsissmi(this), if_rhsisnotsmi(this);
        Branch(TaggedIsSmi(rhs), &if_rhsissmi, &if_rhsisnotsmi);

        BIND(&if_rhsissmi);
        {
          // Convert {lhs} and {rhs} to floating point values.
          TNode<Float64T> lhs_value = LoadHeapNumberValue(CAST(lhs));
          TNode<Float64T> rhs_value = SmiToFloat64(CAST(rhs));

          CombineFeedback(var_type_feedback, CompareOperationFeedback::kNumber);

          // Perform a floating point comparison of {lhs} and {rhs}.
          Branch(Float64Equal(lhs_value, rhs_value), &if_equal, &if_notequal);
        }

        BIND(&if_rhsisnotsmi);
        {
          TNode<HeapObject> rhs_ho = CAST(rhs);
          // Load the map of {rhs}.
          TNode<Map> rhs_map = LoadMap(rhs_ho);

          // Check if {rhs} is also a HeapNumber.
          Label if_rhsisnumber(this), if_rhsisnotnumber(this);
          Branch(IsHeapNumberMap(rhs_map), &if_rhsisnumber, &if_rhsisnotnumber);

          BIND(&if_rhsisnumber);
          {
            // Convert {lhs} and {rhs} to floating point values.
            TNode<Float64T> lhs_value = LoadHeapNumberValue(CAST(lhs));
            TNode<Float64T> rhs_value = LoadHeapNumberValue(CAST(rhs));

            CombineFeedback(var_type_feedback,
                            CompareOperationFeedback::kNumber);

            // Perform a floating point comparison of {lhs} and {rhs}.
            Branch(Float64Equal(lhs_value, rhs_value), &if_equal, &if_notequal);
          }

          BIND(&if_rhsisnotnumber);
          Goto(&if_not_equivalent_types);
        }
      }

      BIND(&if_lhsisnotnumber);
      {
        // Check if {rhs} is a Smi or a HeapObject.
        Label if_rhsissmi(this), if_rhsisnotsmi(this);
        Branch(TaggedIsSmi(rhs), &if_rhsissmi, &if_rhsisnotsmi);

        BIND(&if_rhsissmi);
        Goto(&if_not_equivalent_types);

        BIND(&if_rhsisnotsmi);
        {
          // Load the instance type of {lhs}.
          TNode<Uint16T> lhs_instance_type = LoadMapInstanceType(lhs_map);

          // Check if {lhs} is a String.
          Label if_lhsisstring(this, Label::kDeferred), if_lhsisnotstring(this);
          Branch(IsStringInstanceType(lhs_instance_type), &if_lhsisstring,
                 &if_lhsisnotstring);

          BIND(&if_lhsisstring);
          {
            // Load the instance type of {rhs}.
            TNode<Uint16T> rhs_instance_type = LoadInstanceType(CAST(rhs));

            // Check if {rhs} is also a String.
            Label if_rhsisstring(this, Label::kDeferred),
                if_rhsisnotstring(this);
            Branch(IsStringInstanceType(rhs_instance_type), &if_rhsisstring,
                   &if_rhsisnotstring);

            BIND(&if_rhsisstring);
            {
              if (var_type_feedback != nullptr) {
                TNode<Smi> lhs_feedback =
                    CollectFeedbackForString(lhs_instance_type);
                TNode<Smi> rhs_feedback =
                    CollectFeedbackForString(rhs_instance_type);
                var_type_feedback->Bind(SmiOr(lhs_feedback, rhs_feedback));
              }
              result = CAST(CallBuiltin(Builtins::kStringEqual,
                                        NoContextConstant(), lhs, rhs));
              Goto(&end);
            }

            BIND(&if_rhsisnotstring);
            Goto(&if_not_equivalent_types);
          }

          BIND(&if_lhsisnotstring);
          {
            // Check if {lhs} is a BigInt.
            Label if_lhsisbigint(this), if_lhsisnotbigint(this);
            Branch(IsBigIntInstanceType(lhs_instance_type), &if_lhsisbigint,
                   &if_lhsisnotbigint);

            BIND(&if_lhsisbigint);
            {
              // Load the instance type of {rhs}.
              TNode<Uint16T> rhs_instance_type = LoadInstanceType(CAST(rhs));

              // Check if {rhs} is also a BigInt.
              Label if_rhsisbigint(this, Label::kDeferred),
                  if_rhsisnotbigint(this);
              Branch(IsBigIntInstanceType(rhs_instance_type), &if_rhsisbigint,
                     &if_rhsisnotbigint);

              BIND(&if_rhsisbigint);
              {
                CombineFeedback(var_type_feedback,
                                CompareOperationFeedback::kBigInt);
                result = CAST(CallRuntime(Runtime::kBigIntEqualToBigInt,
                                          NoContextConstant(), lhs, rhs));
                Goto(&end);
              }

              BIND(&if_rhsisnotbigint);
              Goto(&if_not_equivalent_types);
            }

            BIND(&if_lhsisnotbigint);
            if (var_type_feedback != nullptr) {
              // Load the instance type of {rhs}.
              TNode<Map> rhs_map = LoadMap(CAST(rhs));
              TNode<Uint16T> rhs_instance_type = LoadMapInstanceType(rhs_map);

              Label if_lhsissymbol(this), if_lhsisreceiver(this),
                  if_lhsisoddball(this);
              GotoIf(IsJSReceiverInstanceType(lhs_instance_type),
                     &if_lhsisreceiver);
              GotoIf(IsBooleanMap(lhs_map), &if_not_equivalent_types);
              GotoIf(IsOddballInstanceType(lhs_instance_type),
                     &if_lhsisoddball);
              Branch(IsSymbolInstanceType(lhs_instance_type), &if_lhsissymbol,
                     &if_not_equivalent_types);

              BIND(&if_lhsisreceiver);
              {
                GotoIf(IsBooleanMap(rhs_map), &if_not_equivalent_types);
                OverwriteFeedback(var_type_feedback,
                                  CompareOperationFeedback::kReceiver);
                GotoIf(IsJSReceiverInstanceType(rhs_instance_type),
                       &if_notequal);
                OverwriteFeedback(
                    var_type_feedback,
                    CompareOperationFeedback::kReceiverOrNullOrUndefined);
                GotoIf(IsOddballInstanceType(rhs_instance_type), &if_notequal);
                Goto(&if_not_equivalent_types);
              }

              BIND(&if_lhsisoddball);
              {
                STATIC_ASSERT(LAST_PRIMITIVE_TYPE == ODDBALL_TYPE);
                GotoIf(IsBooleanMap(rhs_map), &if_not_equivalent_types);
                GotoIf(Int32LessThan(rhs_instance_type,
                                     Int32Constant(ODDBALL_TYPE)),
                       &if_not_equivalent_types);
                OverwriteFeedback(
                    var_type_feedback,
                    CompareOperationFeedback::kReceiverOrNullOrUndefined);
                Goto(&if_notequal);
              }

              BIND(&if_lhsissymbol);
              {
                GotoIfNot(IsSymbolInstanceType(rhs_instance_type),
                          &if_not_equivalent_types);
                OverwriteFeedback(var_type_feedback,
                                  CompareOperationFeedback::kSymbol);
                Goto(&if_notequal);
              }
            } else {
              Goto(&if_notequal);
            }
          }
        }
      }
    }

    BIND(&if_lhsissmi);
    {
      // We already know that {lhs} and {rhs} are not reference equal, and {lhs}
      // is a Smi; so {lhs} and {rhs} can only be strictly equal if {rhs} is a
      // HeapNumber with an equal floating point value.

      // Check if {rhs} is a Smi or a HeapObject.
      Label if_rhsissmi(this), if_rhsisnotsmi(this);
      Branch(TaggedIsSmi(rhs), &if_rhsissmi, &if_rhsisnotsmi);

      BIND(&if_rhsissmi);
      CombineFeedback(var_type_feedback,
                      CompareOperationFeedback::kSignedSmall);
      Goto(&if_notequal);

      BIND(&if_rhsisnotsmi);
      {
        // Load the map of the {rhs}.
        TNode<Map> rhs_map = LoadMap(CAST(rhs));

        // The {rhs} could be a HeapNumber with the same value as {lhs}.
        Label if_rhsisnumber(this), if_rhsisnotnumber(this);
        Branch(IsHeapNumberMap(rhs_map), &if_rhsisnumber, &if_rhsisnotnumber);

        BIND(&if_rhsisnumber);
        {
          // Convert {lhs} and {rhs} to floating point values.
          TNode<Float64T> lhs_value = SmiToFloat64(CAST(lhs));
          TNode<Float64T> rhs_value = LoadHeapNumberValue(CAST(rhs));

          CombineFeedback(var_type_feedback, CompareOperationFeedback::kNumber);

          // Perform a floating point comparison of {lhs} and {rhs}.
          Branch(Float64Equal(lhs_value, rhs_value), &if_equal, &if_notequal);
        }

        BIND(&if_rhsisnotnumber);
        Goto(&if_not_equivalent_types);
      }
    }
  }

  BIND(&if_equal);
  {
    result = TrueConstant();
    Goto(&end);
  }

  BIND(&if_not_equivalent_types);
  {
    OverwriteFeedback(var_type_feedback, CompareOperationFeedback::kAny);
    Goto(&if_notequal);
  }

  BIND(&if_notequal);
  {
    result = FalseConstant();
    Goto(&end);
  }

  BIND(&end);
  return result.value();
}

// ECMA#sec-samevalue
// This algorithm differs from the Strict Equality Comparison Algorithm in its
// treatment of signed zeroes and NaNs.
void CodeStubAssembler::BranchIfSameValue(SloppyTNode<Object> lhs,
                                          SloppyTNode<Object> rhs,
                                          Label* if_true, Label* if_false,
                                          SameValueMode mode) {
  VARIABLE(var_lhs_value, MachineRepresentation::kFloat64);
  VARIABLE(var_rhs_value, MachineRepresentation::kFloat64);
  Label do_fcmp(this);

  // Immediately jump to {if_true} if {lhs} == {rhs}, because - unlike
  // StrictEqual - SameValue considers two NaNs to be equal.
  GotoIf(TaggedEqual(lhs, rhs), if_true);

  // Check if the {lhs} is a Smi.
  Label if_lhsissmi(this), if_lhsisheapobject(this);
  Branch(TaggedIsSmi(lhs), &if_lhsissmi, &if_lhsisheapobject);

  BIND(&if_lhsissmi);
  {
    // Since {lhs} is a Smi, the comparison can only yield true
    // iff the {rhs} is a HeapNumber with the same float64 value.
    Branch(TaggedIsSmi(rhs), if_false, [&] {
      GotoIfNot(IsHeapNumber(CAST(rhs)), if_false);
      var_lhs_value.Bind(SmiToFloat64(CAST(lhs)));
      var_rhs_value.Bind(LoadHeapNumberValue(CAST(rhs)));
      Goto(&do_fcmp);
    });
  }

  BIND(&if_lhsisheapobject);
  {
    // Check if the {rhs} is a Smi.
    Branch(
        TaggedIsSmi(rhs),
        [&] {
          // Since {rhs} is a Smi, the comparison can only yield true
          // iff the {lhs} is a HeapNumber with the same float64 value.
          GotoIfNot(IsHeapNumber(CAST(lhs)), if_false);
          var_lhs_value.Bind(LoadHeapNumberValue(CAST(lhs)));
          var_rhs_value.Bind(SmiToFloat64(CAST(rhs)));
          Goto(&do_fcmp);
        },
        [&] {
          // Now this can only yield true if either both {lhs} and {rhs} are
          // HeapNumbers with the same value, or both are Strings with the
          // same character sequence, or both are BigInts with the same
          // value.
          Label if_lhsisheapnumber(this), if_lhsisstring(this),
              if_lhsisbigint(this);
          TNode<Map> const lhs_map = LoadMap(CAST(lhs));
          GotoIf(IsHeapNumberMap(lhs_map), &if_lhsisheapnumber);
          if (mode != SameValueMode::kNumbersOnly) {
            TNode<Uint16T> const lhs_instance_type =
                LoadMapInstanceType(lhs_map);
            GotoIf(IsStringInstanceType(lhs_instance_type), &if_lhsisstring);
            GotoIf(IsBigIntInstanceType(lhs_instance_type), &if_lhsisbigint);
          }
          Goto(if_false);

          BIND(&if_lhsisheapnumber);
          {
            GotoIfNot(IsHeapNumber(CAST(rhs)), if_false);
            var_lhs_value.Bind(LoadHeapNumberValue(CAST(lhs)));
            var_rhs_value.Bind(LoadHeapNumberValue(CAST(rhs)));
            Goto(&do_fcmp);
          }

          if (mode != SameValueMode::kNumbersOnly) {
            BIND(&if_lhsisstring);
            {
              // Now we can only yield true if {rhs} is also a String
              // with the same sequence of characters.
              GotoIfNot(IsString(CAST(rhs)), if_false);
              TNode<Object> const result = CallBuiltin(
                  Builtins::kStringEqual, NoContextConstant(), lhs, rhs);
              Branch(IsTrue(result), if_true, if_false);
            }

            BIND(&if_lhsisbigint);
            {
              GotoIfNot(IsBigInt(CAST(rhs)), if_false);
              TNode<Object> const result = CallRuntime(
                  Runtime::kBigIntEqualToBigInt, NoContextConstant(), lhs, rhs);
              Branch(IsTrue(result), if_true, if_false);
            }
          }
        });
  }

  BIND(&do_fcmp);
  {
    TNode<Float64T> lhs_value = UncheckedCast<Float64T>(var_lhs_value.value());
    TNode<Float64T> rhs_value = UncheckedCast<Float64T>(var_rhs_value.value());
    BranchIfSameNumberValue(lhs_value, rhs_value, if_true, if_false);
  }
}

void CodeStubAssembler::BranchIfSameNumberValue(TNode<Float64T> lhs_value,
                                                TNode<Float64T> rhs_value,
                                                Label* if_true,
                                                Label* if_false) {
  Label if_equal(this), if_notequal(this);
  Branch(Float64Equal(lhs_value, rhs_value), &if_equal, &if_notequal);

  BIND(&if_equal);
  {
    // We still need to handle the case when {lhs} and {rhs} are -0.0 and
    // 0.0 (or vice versa). Compare the high word to
    // distinguish between the two.
    TNode<Uint32T> const lhs_hi_word = Float64ExtractHighWord32(lhs_value);
    TNode<Uint32T> const rhs_hi_word = Float64ExtractHighWord32(rhs_value);

    // If x is +0 and y is -0, return false.
    // If x is -0 and y is +0, return false.
    Branch(Word32Equal(lhs_hi_word, rhs_hi_word), if_true, if_false);
  }

  BIND(&if_notequal);
  {
    // Return true iff both {rhs} and {lhs} are NaN.
    GotoIf(Float64Equal(lhs_value, lhs_value), if_false);
    Branch(Float64Equal(rhs_value, rhs_value), if_false, if_true);
  }
}

TNode<Oddball> CodeStubAssembler::HasProperty(SloppyTNode<Context> context,
                                              SloppyTNode<Object> object,
                                              SloppyTNode<Object> key,
                                              HasPropertyLookupMode mode) {
  Label call_runtime(this, Label::kDeferred), return_true(this),
      return_false(this), end(this), if_proxy(this, Label::kDeferred);

  CodeStubAssembler::LookupInHolder lookup_property_in_holder =
      [this, &return_true](Node* receiver, Node* holder, Node* holder_map,
                           Node* holder_instance_type, Node* unique_name,
                           Label* next_holder, Label* if_bailout) {
        TryHasOwnProperty(holder, holder_map, holder_instance_type, unique_name,
                          &return_true, next_holder, if_bailout);
      };

  CodeStubAssembler::LookupInHolder lookup_element_in_holder =
      [this, &return_true, &return_false](
          Node* receiver, Node* holder, Node* holder_map,
          Node* holder_instance_type, Node* index, Label* next_holder,
          Label* if_bailout) {
        TryLookupElement(holder, holder_map, holder_instance_type, index,
                         &return_true, &return_false, next_holder, if_bailout);
      };

  TryPrototypeChainLookup(object, object, key, lookup_property_in_holder,
                          lookup_element_in_holder, &return_false,
                          &call_runtime, &if_proxy);

  TVARIABLE(Oddball, result);

  BIND(&if_proxy);
  {
    TNode<Name> name = CAST(CallBuiltin(Builtins::kToName, context, key));
    switch (mode) {
      case kHasProperty:
        GotoIf(IsPrivateSymbol(name), &return_false);

        result = CAST(
            CallBuiltin(Builtins::kProxyHasProperty, context, object, name));
        Goto(&end);
        break;
      case kForInHasProperty:
        Goto(&call_runtime);
        break;
    }
  }

  BIND(&return_true);
  {
    result = TrueConstant();
    Goto(&end);
  }

  BIND(&return_false);
  {
    result = FalseConstant();
    Goto(&end);
  }

  BIND(&call_runtime);
  {
    Runtime::FunctionId fallback_runtime_function_id;
    switch (mode) {
      case kHasProperty:
        fallback_runtime_function_id = Runtime::kHasProperty;
        break;
      case kForInHasProperty:
        fallback_runtime_function_id = Runtime::kForInHasProperty;
        break;
    }

    result =
        CAST(CallRuntime(fallback_runtime_function_id, context, object, key));
    Goto(&end);
  }

  BIND(&end);
  CSA_ASSERT(this, IsBoolean(result.value()));
  return result.value();
}

Node* CodeStubAssembler::Typeof(Node* value) {
  VARIABLE(result_var, MachineRepresentation::kTagged);

  Label return_number(this, Label::kDeferred), if_oddball(this),
      return_function(this), return_undefined(this), return_object(this),
      return_string(this), return_bigint(this), return_result(this);

  GotoIf(TaggedIsSmi(value), &return_number);

  TNode<Map> map = LoadMap(value);

  GotoIf(IsHeapNumberMap(map), &return_number);

  TNode<Uint16T> instance_type = LoadMapInstanceType(map);

  GotoIf(InstanceTypeEqual(instance_type, ODDBALL_TYPE), &if_oddball);

  TNode<Int32T> callable_or_undetectable_mask = Word32And(
      LoadMapBitField(map),
      Int32Constant(Map::IsCallableBit::kMask | Map::IsUndetectableBit::kMask));

  GotoIf(Word32Equal(callable_or_undetectable_mask,
                     Int32Constant(Map::IsCallableBit::kMask)),
         &return_function);

  GotoIfNot(Word32Equal(callable_or_undetectable_mask, Int32Constant(0)),
            &return_undefined);

  GotoIf(IsJSReceiverInstanceType(instance_type), &return_object);

  GotoIf(IsStringInstanceType(instance_type), &return_string);

  GotoIf(IsBigIntInstanceType(instance_type), &return_bigint);

  CSA_ASSERT(this, InstanceTypeEqual(instance_type, SYMBOL_TYPE));
  result_var.Bind(HeapConstant(isolate()->factory()->symbol_string()));
  Goto(&return_result);

  BIND(&return_number);
  {
    result_var.Bind(HeapConstant(isolate()->factory()->number_string()));
    Goto(&return_result);
  }

  BIND(&if_oddball);
  {
    TNode<Object> type = LoadObjectField(value, Oddball::kTypeOfOffset);
    result_var.Bind(type);
    Goto(&return_result);
  }

  BIND(&return_function);
  {
    result_var.Bind(HeapConstant(isolate()->factory()->function_string()));
    Goto(&return_result);
  }

  BIND(&return_undefined);
  {
    result_var.Bind(HeapConstant(isolate()->factory()->undefined_string()));
    Goto(&return_result);
  }

  BIND(&return_object);
  {
    result_var.Bind(HeapConstant(isolate()->factory()->object_string()));
    Goto(&return_result);
  }

  BIND(&return_string);
  {
    result_var.Bind(HeapConstant(isolate()->factory()->string_string()));
    Goto(&return_result);
  }

  BIND(&return_bigint);
  {
    result_var.Bind(HeapConstant(isolate()->factory()->bigint_string()));
    Goto(&return_result);
  }

  BIND(&return_result);
  return result_var.value();
}

TNode<Object> CodeStubAssembler::GetSuperConstructor(
    SloppyTNode<Context> context, SloppyTNode<JSFunction> active_function) {
  Label is_not_constructor(this, Label::kDeferred), out(this);
  TVARIABLE(Object, result);

  TNode<Map> map = LoadMap(active_function);
  TNode<HeapObject> prototype = LoadMapPrototype(map);
  TNode<Map> prototype_map = LoadMap(prototype);
  GotoIfNot(IsConstructorMap(prototype_map), &is_not_constructor);

  result = prototype;
  Goto(&out);

  BIND(&is_not_constructor);
  {
    CallRuntime(Runtime::kThrowNotSuperConstructor, context, prototype,
                active_function);
    Unreachable();
  }

  BIND(&out);
  return result.value();
}

TNode<JSReceiver> CodeStubAssembler::SpeciesConstructor(
    SloppyTNode<Context> context, SloppyTNode<Object> object,
    SloppyTNode<JSReceiver> default_constructor) {
  Isolate* isolate = this->isolate();
  TVARIABLE(JSReceiver, var_result, default_constructor);

  // 2. Let C be ? Get(O, "constructor").
  TNode<Object> constructor =
      GetProperty(context, object, isolate->factory()->constructor_string());

  // 3. If C is undefined, return defaultConstructor.
  Label out(this);
  GotoIf(IsUndefined(constructor), &out);

  // 4. If Type(C) is not Object, throw a TypeError exception.
  ThrowIfNotJSReceiver(context, constructor,
                       MessageTemplate::kConstructorNotReceiver, "");

  // 5. Let S be ? Get(C, @@species).
  TNode<Object> species =
      GetProperty(context, constructor, isolate->factory()->species_symbol());

  // 6. If S is either undefined or null, return defaultConstructor.
  GotoIf(IsNullOrUndefined(species), &out);

  // 7. If IsConstructor(S) is true, return S.
  Label throw_error(this);
  GotoIf(TaggedIsSmi(species), &throw_error);
  GotoIfNot(IsConstructorMap(LoadMap(CAST(species))), &throw_error);
  var_result = CAST(species);
  Goto(&out);

  // 8. Throw a TypeError exception.
  BIND(&throw_error);
  ThrowTypeError(context, MessageTemplate::kSpeciesNotConstructor);

  BIND(&out);
  return var_result.value();
}

Node* CodeStubAssembler::InstanceOf(Node* object, Node* callable,
                                    Node* context) {
  VARIABLE(var_result, MachineRepresentation::kTagged);
  Label if_notcallable(this, Label::kDeferred),
      if_notreceiver(this, Label::kDeferred), if_otherhandler(this),
      if_nohandler(this, Label::kDeferred), return_true(this),
      return_false(this), return_result(this, &var_result);

  // Ensure that the {callable} is actually a JSReceiver.
  GotoIf(TaggedIsSmi(callable), &if_notreceiver);
  GotoIfNot(IsJSReceiver(callable), &if_notreceiver);

  // Load the @@hasInstance property from {callable}.
  TNode<Object> inst_of_handler =
      GetProperty(context, callable, HasInstanceSymbolConstant());

  // Optimize for the likely case where {inst_of_handler} is the builtin
  // Function.prototype[@@hasInstance] method, and emit a direct call in
  // that case without any additional checking.
  TNode<NativeContext> native_context = LoadNativeContext(context);
  TNode<Object> function_has_instance =
      LoadContextElement(native_context, Context::FUNCTION_HAS_INSTANCE_INDEX);
  GotoIfNot(TaggedEqual(inst_of_handler, function_has_instance),
            &if_otherhandler);
  {
    // Call to Function.prototype[@@hasInstance] directly.
    Callable builtin(BUILTIN_CODE(isolate(), FunctionPrototypeHasInstance),
                     CallTrampolineDescriptor{});
    Node* result = CallJS(builtin, context, inst_of_handler, callable, object);
    var_result.Bind(result);
    Goto(&return_result);
  }

  BIND(&if_otherhandler);
  {
    // Check if there's actually an {inst_of_handler}.
    GotoIf(IsNull(inst_of_handler), &if_nohandler);
    GotoIf(IsUndefined(inst_of_handler), &if_nohandler);

    // Call the {inst_of_handler} for {callable} and {object}.
    Node* result = CallJS(
        CodeFactory::Call(isolate(), ConvertReceiverMode::kNotNullOrUndefined),
        context, inst_of_handler, callable, object);

    // Convert the {result} to a Boolean.
    BranchIfToBooleanIsTrue(result, &return_true, &return_false);
  }

  BIND(&if_nohandler);
  {
    // Ensure that the {callable} is actually Callable.
    GotoIfNot(IsCallable(callable), &if_notcallable);

    // Use the OrdinaryHasInstance algorithm.
    TNode<Object> result =
        CallBuiltin(Builtins::kOrdinaryHasInstance, context, callable, object);
    var_result.Bind(result);
    Goto(&return_result);
  }

  BIND(&if_notcallable);
  { ThrowTypeError(context, MessageTemplate::kNonCallableInInstanceOfCheck); }

  BIND(&if_notreceiver);
  { ThrowTypeError(context, MessageTemplate::kNonObjectInInstanceOfCheck); }

  BIND(&return_true);
  var_result.Bind(TrueConstant());
  Goto(&return_result);

  BIND(&return_false);
  var_result.Bind(FalseConstant());
  Goto(&return_result);

  BIND(&return_result);
  return var_result.value();
}

TNode<Number> CodeStubAssembler::NumberInc(SloppyTNode<Number> value) {
  TVARIABLE(Number, var_result);
  TVARIABLE(Float64T, var_finc_value);
  Label if_issmi(this), if_isnotsmi(this), do_finc(this), end(this);
  Branch(TaggedIsSmi(value), &if_issmi, &if_isnotsmi);

  BIND(&if_issmi);
  {
    Label if_overflow(this);
    TNode<Smi> smi_value = CAST(value);
    TNode<Smi> one = SmiConstant(1);
    var_result = TrySmiAdd(smi_value, one, &if_overflow);
    Goto(&end);

    BIND(&if_overflow);
    {
      var_finc_value = SmiToFloat64(smi_value);
      Goto(&do_finc);
    }
  }

  BIND(&if_isnotsmi);
  {
    TNode<HeapNumber> heap_number_value = CAST(value);

    // Load the HeapNumber value.
    var_finc_value = LoadHeapNumberValue(heap_number_value);
    Goto(&do_finc);
  }

  BIND(&do_finc);
  {
    TNode<Float64T> finc_value = var_finc_value.value();
    TNode<Float64T> one = Float64Constant(1.0);
    TNode<Float64T> finc_result = Float64Add(finc_value, one);
    var_result = AllocateHeapNumberWithValue(finc_result);
    Goto(&end);
  }

  BIND(&end);
  return var_result.value();
}

TNode<Number> CodeStubAssembler::NumberDec(SloppyTNode<Number> value) {
  TVARIABLE(Number, var_result);
  TVARIABLE(Float64T, var_fdec_value);
  Label if_issmi(this), if_isnotsmi(this), do_fdec(this), end(this);
  Branch(TaggedIsSmi(value), &if_issmi, &if_isnotsmi);

  BIND(&if_issmi);
  {
    TNode<Smi> smi_value = CAST(value);
    TNode<Smi> one = SmiConstant(1);
    Label if_overflow(this);
    var_result = TrySmiSub(smi_value, one, &if_overflow);
    Goto(&end);

    BIND(&if_overflow);
    {
      var_fdec_value = SmiToFloat64(smi_value);
      Goto(&do_fdec);
    }
  }

  BIND(&if_isnotsmi);
  {
    TNode<HeapNumber> heap_number_value = CAST(value);

    // Load the HeapNumber value.
    var_fdec_value = LoadHeapNumberValue(heap_number_value);
    Goto(&do_fdec);
  }

  BIND(&do_fdec);
  {
    TNode<Float64T> fdec_value = var_fdec_value.value();
    TNode<Float64T> minus_one = Float64Constant(-1.0);
    TNode<Float64T> fdec_result = Float64Add(fdec_value, minus_one);
    var_result = AllocateHeapNumberWithValue(fdec_result);
    Goto(&end);
  }

  BIND(&end);
  return var_result.value();
}

TNode<Number> CodeStubAssembler::NumberAdd(SloppyTNode<Number> a,
                                           SloppyTNode<Number> b) {
  TVARIABLE(Number, var_result);
  Label float_add(this, Label::kDeferred), end(this);
  GotoIf(TaggedIsNotSmi(a), &float_add);
  GotoIf(TaggedIsNotSmi(b), &float_add);

  // Try fast Smi addition first.
  var_result = TrySmiAdd(CAST(a), CAST(b), &float_add);
  Goto(&end);

  BIND(&float_add);
  {
    var_result = ChangeFloat64ToTagged(
        Float64Add(ChangeNumberToFloat64(a), ChangeNumberToFloat64(b)));
    Goto(&end);
  }

  BIND(&end);
  return var_result.value();
}

TNode<Number> CodeStubAssembler::NumberSub(SloppyTNode<Number> a,
                                           SloppyTNode<Number> b) {
  TVARIABLE(Number, var_result);
  Label float_sub(this, Label::kDeferred), end(this);
  GotoIf(TaggedIsNotSmi(a), &float_sub);
  GotoIf(TaggedIsNotSmi(b), &float_sub);

  // Try fast Smi subtraction first.
  var_result = TrySmiSub(CAST(a), CAST(b), &float_sub);
  Goto(&end);

  BIND(&float_sub);
  {
    var_result = ChangeFloat64ToTagged(
        Float64Sub(ChangeNumberToFloat64(a), ChangeNumberToFloat64(b)));
    Goto(&end);
  }

  BIND(&end);
  return var_result.value();
}

void CodeStubAssembler::GotoIfNotNumber(Node* input, Label* is_not_number) {
  Label is_number(this);
  GotoIf(TaggedIsSmi(input), &is_number);
  Branch(IsHeapNumber(input), &is_number, is_not_number);
  BIND(&is_number);
}

void CodeStubAssembler::GotoIfNumber(Node* input, Label* is_number) {
  GotoIf(TaggedIsSmi(input), is_number);
  GotoIf(IsHeapNumber(input), is_number);
}

TNode<Number> CodeStubAssembler::BitwiseOp(Node* left32, Node* right32,
                                           Operation bitwise_op) {
  switch (bitwise_op) {
    case Operation::kBitwiseAnd:
      return ChangeInt32ToTagged(Signed(Word32And(left32, right32)));
    case Operation::kBitwiseOr:
      return ChangeInt32ToTagged(Signed(Word32Or(left32, right32)));
    case Operation::kBitwiseXor:
      return ChangeInt32ToTagged(Signed(Word32Xor(left32, right32)));
    case Operation::kShiftLeft:
      if (!Word32ShiftIsSafe()) {
        right32 = Word32And(right32, Int32Constant(0x1F));
      }
      return ChangeInt32ToTagged(Signed(Word32Shl(left32, right32)));
    case Operation::kShiftRight:
      if (!Word32ShiftIsSafe()) {
        right32 = Word32And(right32, Int32Constant(0x1F));
      }
      return ChangeInt32ToTagged(Signed(Word32Sar(left32, right32)));
    case Operation::kShiftRightLogical:
      if (!Word32ShiftIsSafe()) {
        right32 = Word32And(right32, Int32Constant(0x1F));
      }
      return ChangeUint32ToTagged(Unsigned(Word32Shr(left32, right32)));
    default:
      break;
  }
  UNREACHABLE();
}

// ES #sec-createarrayiterator
TNode<JSArrayIterator> CodeStubAssembler::CreateArrayIterator(
    TNode<Context> context, TNode<Object> object, IterationKind kind) {
  TNode<NativeContext> native_context = LoadNativeContext(context);
  TNode<Map> iterator_map = CAST(LoadContextElement(
      native_context, Context::INITIAL_ARRAY_ITERATOR_MAP_INDEX));
  TNode<HeapObject> iterator = Allocate(JSArrayIterator::kSize);
  StoreMapNoWriteBarrier(iterator, iterator_map);
  StoreObjectFieldRoot(iterator, JSArrayIterator::kPropertiesOrHashOffset,
                       RootIndex::kEmptyFixedArray);
  StoreObjectFieldRoot(iterator, JSArrayIterator::kElementsOffset,
                       RootIndex::kEmptyFixedArray);
  StoreObjectFieldNoWriteBarrier(
      iterator, JSArrayIterator::kIteratedObjectOffset, object);
  StoreObjectFieldNoWriteBarrier(iterator, JSArrayIterator::kNextIndexOffset,
                                 SmiConstant(0));
  StoreObjectFieldNoWriteBarrier(
      iterator, JSArrayIterator::kKindOffset,
      SmiConstant(Smi::FromInt(static_cast<int>(kind))));
  return CAST(iterator);
}

TNode<JSObject> CodeStubAssembler::AllocateJSIteratorResult(
    SloppyTNode<Context> context, SloppyTNode<Object> value,
    SloppyTNode<Oddball> done) {
  CSA_ASSERT(this, IsBoolean(done));
  TNode<NativeContext> native_context = LoadNativeContext(context);
  TNode<Object> map =
      LoadContextElement(native_context, Context::ITERATOR_RESULT_MAP_INDEX);
  TNode<HeapObject> result = Allocate(JSIteratorResult::kSize);
  StoreMapNoWriteBarrier(result, map);
  StoreObjectFieldRoot(result, JSIteratorResult::kPropertiesOrHashOffset,
                       RootIndex::kEmptyFixedArray);
  StoreObjectFieldRoot(result, JSIteratorResult::kElementsOffset,
                       RootIndex::kEmptyFixedArray);
  StoreObjectFieldNoWriteBarrier(result, JSIteratorResult::kValueOffset, value);
  StoreObjectFieldNoWriteBarrier(result, JSIteratorResult::kDoneOffset, done);
  return CAST(result);
}

Node* CodeStubAssembler::AllocateJSIteratorResultForEntry(Node* context,
                                                          Node* key,
                                                          Node* value) {
  TNode<NativeContext> native_context = LoadNativeContext(context);
  TNode<Smi> length = SmiConstant(2);
  int const elements_size = FixedArray::SizeFor(2);
  TNode<FixedArray> elements = UncheckedCast<FixedArray>(
      Allocate(elements_size + JSArray::kSize + JSIteratorResult::kSize));
  StoreObjectFieldRoot(elements, FixedArray::kMapOffset,
                       RootIndex::kFixedArrayMap);
  StoreObjectFieldNoWriteBarrier(elements, FixedArray::kLengthOffset, length);
  StoreFixedArrayElement(elements, 0, key);
  StoreFixedArrayElement(elements, 1, value);
  TNode<Object> array_map = LoadContextElement(
      native_context, Context::JS_ARRAY_PACKED_ELEMENTS_MAP_INDEX);
  TNode<HeapObject> array = InnerAllocate(elements, elements_size);
  StoreMapNoWriteBarrier(array, array_map);
  StoreObjectFieldRoot(array, JSArray::kPropertiesOrHashOffset,
                       RootIndex::kEmptyFixedArray);
  StoreObjectFieldNoWriteBarrier(array, JSArray::kElementsOffset, elements);
  StoreObjectFieldNoWriteBarrier(array, JSArray::kLengthOffset, length);
  TNode<Object> iterator_map =
      LoadContextElement(native_context, Context::ITERATOR_RESULT_MAP_INDEX);
  TNode<HeapObject> result = InnerAllocate(array, JSArray::kSize);
  StoreMapNoWriteBarrier(result, iterator_map);
  StoreObjectFieldRoot(result, JSIteratorResult::kPropertiesOrHashOffset,
                       RootIndex::kEmptyFixedArray);
  StoreObjectFieldRoot(result, JSIteratorResult::kElementsOffset,
                       RootIndex::kEmptyFixedArray);
  StoreObjectFieldNoWriteBarrier(result, JSIteratorResult::kValueOffset, array);
  StoreObjectFieldRoot(result, JSIteratorResult::kDoneOffset,
                       RootIndex::kFalseValue);
  return result;
}

TNode<JSReceiver> CodeStubAssembler::ArraySpeciesCreate(TNode<Context> context,
                                                        TNode<Object> o,
                                                        TNode<Number> len) {
  TNode<JSReceiver> constructor =
      CAST(CallRuntime(Runtime::kArraySpeciesConstructor, context, o));
  return Construct(context, constructor, len);
}

TNode<BoolT> CodeStubAssembler::IsDetachedBuffer(TNode<JSArrayBuffer> buffer) {
  TNode<Uint32T> buffer_bit_field = LoadJSArrayBufferBitField(buffer);
  return IsSetWord32<JSArrayBuffer::WasDetachedBit>(buffer_bit_field);
}

void CodeStubAssembler::ThrowIfArrayBufferIsDetached(
    SloppyTNode<Context> context, TNode<JSArrayBuffer> array_buffer,
    const char* method_name) {
  Label if_detached(this, Label::kDeferred), if_not_detached(this);
  Branch(IsDetachedBuffer(array_buffer), &if_detached, &if_not_detached);
  BIND(&if_detached);
  ThrowTypeError(context, MessageTemplate::kDetachedOperation, method_name);
  BIND(&if_not_detached);
}

void CodeStubAssembler::ThrowIfArrayBufferViewBufferIsDetached(
    SloppyTNode<Context> context, TNode<JSArrayBufferView> array_buffer_view,
    const char* method_name) {
  TNode<JSArrayBuffer> buffer = LoadJSArrayBufferViewBuffer(array_buffer_view);
  ThrowIfArrayBufferIsDetached(context, buffer, method_name);
}

TNode<Uint32T> CodeStubAssembler::LoadJSArrayBufferBitField(
    TNode<JSArrayBuffer> array_buffer) {
  return LoadObjectField<Uint32T>(array_buffer, JSArrayBuffer::kBitFieldOffset);
}

TNode<RawPtrT> CodeStubAssembler::LoadJSArrayBufferBackingStore(
    TNode<JSArrayBuffer> array_buffer) {
  return LoadObjectField<RawPtrT>(array_buffer,
                                  JSArrayBuffer::kBackingStoreOffset);
}

TNode<JSArrayBuffer> CodeStubAssembler::LoadJSArrayBufferViewBuffer(
    TNode<JSArrayBufferView> array_buffer_view) {
  return LoadObjectField<JSArrayBuffer>(array_buffer_view,
                                        JSArrayBufferView::kBufferOffset);
}

TNode<UintPtrT> CodeStubAssembler::LoadJSArrayBufferViewByteLength(
    TNode<JSArrayBufferView> array_buffer_view) {
  return LoadObjectField<UintPtrT>(array_buffer_view,
                                   JSArrayBufferView::kByteLengthOffset);
}

TNode<UintPtrT> CodeStubAssembler::LoadJSArrayBufferViewByteOffset(
    TNode<JSArrayBufferView> array_buffer_view) {
  return LoadObjectField<UintPtrT>(array_buffer_view,
                                   JSArrayBufferView::kByteOffsetOffset);
}

TNode<UintPtrT> CodeStubAssembler::LoadJSTypedArrayLength(
    TNode<JSTypedArray> typed_array) {
  return LoadObjectField<UintPtrT>(typed_array, JSTypedArray::kLengthOffset);
}

CodeStubArguments::CodeStubArguments(
    CodeStubAssembler* assembler, Node* argc, Node* fp,
    CodeStubAssembler::ParameterMode param_mode, ReceiverMode receiver_mode)
    : assembler_(assembler),
      argc_mode_(param_mode),
      receiver_mode_(receiver_mode),
      argc_(argc),
      base_(),
      fp_(fp != nullptr ? fp : assembler_->LoadFramePointer()) {
  TNode<IntPtrT> offset = assembler_->ElementOffsetFromIndex(
      argc_, SYSTEM_POINTER_ELEMENTS, param_mode,
      (StandardFrameConstants::kFixedSlotCountAboveFp - 1) *
          kSystemPointerSize);
  base_ =
      assembler_->UncheckedCast<RawPtrT>(assembler_->IntPtrAdd(fp_, offset));
}

TNode<Object> CodeStubArguments::GetReceiver() const {
  DCHECK_EQ(receiver_mode_, ReceiverMode::kHasReceiver);
  return assembler_->UncheckedCast<Object>(assembler_->LoadFullTagged(
      base_, assembler_->IntPtrConstant(kSystemPointerSize)));
}

void CodeStubArguments::SetReceiver(TNode<Object> object) const {
  DCHECK_EQ(receiver_mode_, ReceiverMode::kHasReceiver);
  assembler_->StoreFullTaggedNoWriteBarrier(
      base_, assembler_->IntPtrConstant(kSystemPointerSize), object);
}

TNode<WordT> CodeStubArguments::AtIndexPtr(
    Node* index, CodeStubAssembler::ParameterMode mode) const {
  using Node = compiler::Node;
  Node* negated_index = assembler_->IntPtrOrSmiSub(
      assembler_->IntPtrOrSmiConstant(0, mode), index, mode);
  TNode<IntPtrT> offset = assembler_->ElementOffsetFromIndex(
      negated_index, SYSTEM_POINTER_ELEMENTS, mode, 0);
  return assembler_->IntPtrAdd(assembler_->UncheckedCast<IntPtrT>(base_),
                               offset);
}

TNode<Object> CodeStubArguments::AtIndex(
    Node* index, CodeStubAssembler::ParameterMode mode) const {
  DCHECK_EQ(argc_mode_, mode);
  CSA_ASSERT(assembler_,
             assembler_->UintPtrOrSmiLessThan(index, GetLength(mode), mode));
  return assembler_->UncheckedCast<Object>(
      assembler_->LoadFullTagged(AtIndexPtr(index, mode)));
}

TNode<Object> CodeStubArguments::AtIndex(int index) const {
  return AtIndex(assembler_->IntPtrConstant(index));
}

TNode<Object> CodeStubArguments::GetOptionalArgumentValue(
    int index, TNode<Object> default_value) {
  CodeStubAssembler::TVariable<Object> result(assembler_);
  CodeStubAssembler::Label argument_missing(assembler_),
      argument_done(assembler_, &result);

  assembler_->GotoIf(assembler_->UintPtrOrSmiGreaterThanOrEqual(
                         assembler_->IntPtrOrSmiConstant(index, argc_mode_),
                         argc_, argc_mode_),
                     &argument_missing);
  result = AtIndex(index);
  assembler_->Goto(&argument_done);

  assembler_->BIND(&argument_missing);
  result = default_value;
  assembler_->Goto(&argument_done);

  assembler_->BIND(&argument_done);
  return result.value();
}

TNode<Object> CodeStubArguments::GetOptionalArgumentValue(
    TNode<IntPtrT> index, TNode<Object> default_value) {
  CodeStubAssembler::TVariable<Object> result(assembler_);
  CodeStubAssembler::Label argument_missing(assembler_),
      argument_done(assembler_, &result);

  assembler_->GotoIf(
      assembler_->UintPtrOrSmiGreaterThanOrEqual(
          assembler_->IntPtrToParameter(index, argc_mode_), argc_, argc_mode_),
      &argument_missing);
  result = AtIndex(index);
  assembler_->Goto(&argument_done);

  assembler_->BIND(&argument_missing);
  result = default_value;
  assembler_->Goto(&argument_done);

  assembler_->BIND(&argument_done);
  return result.value();
}

void CodeStubArguments::ForEach(
    const CodeStubAssembler::VariableList& vars,
    const CodeStubArguments::ForEachBodyFunction& body, Node* first, Node* last,
    CodeStubAssembler::ParameterMode mode) {
  assembler_->Comment("CodeStubArguments::ForEach");
  if (first == nullptr) {
    first = assembler_->IntPtrOrSmiConstant(0, mode);
  }
  if (last == nullptr) {
    DCHECK_EQ(mode, argc_mode_);
    last = argc_;
  }
  TNode<IntPtrT> start = assembler_->IntPtrSub(
      assembler_->UncheckedCast<IntPtrT>(base_),
      assembler_->ElementOffsetFromIndex(first, SYSTEM_POINTER_ELEMENTS, mode));
  TNode<IntPtrT> end = assembler_->IntPtrSub(
      assembler_->UncheckedCast<IntPtrT>(base_),
      assembler_->ElementOffsetFromIndex(last, SYSTEM_POINTER_ELEMENTS, mode));
  assembler_->BuildFastLoop(
      vars, start, end,
      [this, &body](Node* current) {
        Node* arg = assembler_->Load(MachineType::AnyTagged(), current);
        body(arg);
      },
      -kSystemPointerSize, CodeStubAssembler::INTPTR_PARAMETERS,
      CodeStubAssembler::IndexAdvanceMode::kPost);
}

void CodeStubArguments::PopAndReturn(Node* value) {
  Node* pop_count;
  if (receiver_mode_ == ReceiverMode::kHasReceiver) {
    pop_count = assembler_->IntPtrOrSmiAdd(
        argc_, assembler_->IntPtrOrSmiConstant(1, argc_mode_), argc_mode_);
  } else {
    pop_count = argc_;
  }

  assembler_->PopAndReturn(assembler_->ParameterToIntPtr(pop_count, argc_mode_),
                           value);
}

TNode<BoolT> CodeStubAssembler::IsFastElementsKind(
    TNode<Int32T> elements_kind) {
  STATIC_ASSERT(FIRST_ELEMENTS_KIND == FIRST_FAST_ELEMENTS_KIND);
  return Uint32LessThanOrEqual(elements_kind,
                               Int32Constant(LAST_FAST_ELEMENTS_KIND));
}

TNode<BoolT> CodeStubAssembler::IsDoubleElementsKind(
    TNode<Int32T> elements_kind) {
  STATIC_ASSERT(FIRST_ELEMENTS_KIND == FIRST_FAST_ELEMENTS_KIND);
  STATIC_ASSERT((PACKED_DOUBLE_ELEMENTS & 1) == 0);
  STATIC_ASSERT(PACKED_DOUBLE_ELEMENTS + 1 == HOLEY_DOUBLE_ELEMENTS);
  return Word32Equal(Word32Shr(elements_kind, Int32Constant(1)),
                     Int32Constant(PACKED_DOUBLE_ELEMENTS / 2));
}

TNode<BoolT> CodeStubAssembler::IsFastSmiOrTaggedElementsKind(
    TNode<Int32T> elements_kind) {
  STATIC_ASSERT(FIRST_ELEMENTS_KIND == FIRST_FAST_ELEMENTS_KIND);
  STATIC_ASSERT(PACKED_DOUBLE_ELEMENTS > TERMINAL_FAST_ELEMENTS_KIND);
  STATIC_ASSERT(HOLEY_DOUBLE_ELEMENTS > TERMINAL_FAST_ELEMENTS_KIND);
  return Uint32LessThanOrEqual(elements_kind,
                               Int32Constant(TERMINAL_FAST_ELEMENTS_KIND));
}

TNode<BoolT> CodeStubAssembler::IsFastSmiElementsKind(
    SloppyTNode<Int32T> elements_kind) {
  return Uint32LessThanOrEqual(elements_kind,
                               Int32Constant(HOLEY_SMI_ELEMENTS));
}

TNode<BoolT> CodeStubAssembler::IsHoleyFastElementsKind(
    TNode<Int32T> elements_kind) {
  CSA_ASSERT(this, IsFastElementsKind(elements_kind));

  STATIC_ASSERT(HOLEY_SMI_ELEMENTS == (PACKED_SMI_ELEMENTS | 1));
  STATIC_ASSERT(HOLEY_ELEMENTS == (PACKED_ELEMENTS | 1));
  STATIC_ASSERT(HOLEY_DOUBLE_ELEMENTS == (PACKED_DOUBLE_ELEMENTS | 1));
  return IsSetWord32(elements_kind, 1);
}

TNode<BoolT> CodeStubAssembler::IsHoleyFastElementsKindForRead(
    TNode<Int32T> elements_kind) {
  CSA_ASSERT(this, Uint32LessThanOrEqual(
                       elements_kind,
                       Int32Constant(LAST_ANY_NONEXTENSIBLE_ELEMENTS_KIND)));

  STATIC_ASSERT(HOLEY_SMI_ELEMENTS == (PACKED_SMI_ELEMENTS | 1));
  STATIC_ASSERT(HOLEY_ELEMENTS == (PACKED_ELEMENTS | 1));
  STATIC_ASSERT(HOLEY_DOUBLE_ELEMENTS == (PACKED_DOUBLE_ELEMENTS | 1));
  STATIC_ASSERT(HOLEY_NONEXTENSIBLE_ELEMENTS ==
                (PACKED_NONEXTENSIBLE_ELEMENTS | 1));
  STATIC_ASSERT(HOLEY_SEALED_ELEMENTS == (PACKED_SEALED_ELEMENTS | 1));
  STATIC_ASSERT(HOLEY_FROZEN_ELEMENTS == (PACKED_FROZEN_ELEMENTS | 1));
  return IsSetWord32(elements_kind, 1);
}

TNode<BoolT> CodeStubAssembler::IsElementsKindGreaterThan(
    TNode<Int32T> target_kind, ElementsKind reference_kind) {
  return Int32GreaterThan(target_kind, Int32Constant(reference_kind));
}

TNode<BoolT> CodeStubAssembler::IsElementsKindLessThanOrEqual(
    TNode<Int32T> target_kind, ElementsKind reference_kind) {
  return Int32LessThanOrEqual(target_kind, Int32Constant(reference_kind));
}

TNode<BoolT> CodeStubAssembler::IsElementsKindInRange(
    TNode<Int32T> target_kind, ElementsKind lower_reference_kind,
    ElementsKind higher_reference_kind) {
  return Uint32LessThanOrEqual(
      Int32Sub(target_kind, Int32Constant(lower_reference_kind)),
      Int32Constant(higher_reference_kind - lower_reference_kind));
}

Node* CodeStubAssembler::IsDebugActive() {
  TNode<Uint8T> is_debug_active = Load<Uint8T>(
      ExternalConstant(ExternalReference::debug_is_active_address(isolate())));
  return Word32NotEqual(is_debug_active, Int32Constant(0));
}

Node* CodeStubAssembler::IsPromiseHookEnabled() {
  TNode<RawPtrT> const promise_hook = Load<RawPtrT>(
      ExternalConstant(ExternalReference::promise_hook_address(isolate())));
  return WordNotEqual(promise_hook, IntPtrConstant(0));
}

Node* CodeStubAssembler::HasAsyncEventDelegate() {
  TNode<RawPtrT> const async_event_delegate = Load<RawPtrT>(ExternalConstant(
      ExternalReference::async_event_delegate_address(isolate())));
  return WordNotEqual(async_event_delegate, IntPtrConstant(0));
}

Node* CodeStubAssembler::IsPromiseHookEnabledOrHasAsyncEventDelegate() {
  TNode<Uint8T> const promise_hook_or_async_event_delegate =
      Load<Uint8T>(ExternalConstant(
          ExternalReference::promise_hook_or_async_event_delegate_address(
              isolate())));
  return Word32NotEqual(promise_hook_or_async_event_delegate, Int32Constant(0));
}

Node* CodeStubAssembler::
    IsPromiseHookEnabledOrDebugIsActiveOrHasAsyncEventDelegate() {
  TNode<Uint8T> const promise_hook_or_debug_is_active_or_async_event_delegate =
      Load<Uint8T>(ExternalConstant(
          ExternalReference::
              promise_hook_or_debug_is_active_or_async_event_delegate_address(
                  isolate())));
  return Word32NotEqual(promise_hook_or_debug_is_active_or_async_event_delegate,
                        Int32Constant(0));
}

TNode<Code> CodeStubAssembler::LoadBuiltin(TNode<Smi> builtin_id) {
  CSA_ASSERT(this, SmiGreaterThanOrEqual(builtin_id, SmiConstant(0)));
  CSA_ASSERT(this,
             SmiLessThan(builtin_id, SmiConstant(Builtins::builtin_count)));

  int const kSmiShiftBits = kSmiShiftSize + kSmiTagSize;
  int index_shift = kSystemPointerSizeLog2 - kSmiShiftBits;
  TNode<WordT> table_index =
      index_shift >= 0
          ? WordShl(BitcastTaggedSignedToWord(builtin_id), index_shift)
          : WordSar(BitcastTaggedSignedToWord(builtin_id), -index_shift);

  return CAST(
      Load(MachineType::TaggedPointer(),
           ExternalConstant(ExternalReference::builtins_address(isolate())),
           table_index));
}

TNode<Code> CodeStubAssembler::GetSharedFunctionInfoCode(
    SloppyTNode<SharedFunctionInfo> shared_info, Label* if_compile_lazy) {
  TNode<Object> sfi_data =
      LoadObjectField(shared_info, SharedFunctionInfo::kFunctionDataOffset);

  TVARIABLE(Code, sfi_code);

  Label done(this);
  Label check_instance_type(this);

  // IsSmi: Is builtin
  GotoIf(TaggedIsNotSmi(sfi_data), &check_instance_type);
  if (if_compile_lazy) {
    GotoIf(SmiEqual(CAST(sfi_data), SmiConstant(Builtins::kCompileLazy)),
           if_compile_lazy);
  }
  sfi_code = LoadBuiltin(CAST(sfi_data));
  Goto(&done);

  // Switch on data's instance type.
  BIND(&check_instance_type);
  TNode<Uint16T> data_type = LoadInstanceType(CAST(sfi_data));

  int32_t case_values[] = {BYTECODE_ARRAY_TYPE,
                           WASM_EXPORTED_FUNCTION_DATA_TYPE,
                           ASM_WASM_DATA_TYPE,
                           UNCOMPILED_DATA_WITHOUT_PREPARSE_DATA_TYPE,
                           UNCOMPILED_DATA_WITH_PREPARSE_DATA_TYPE,
                           FUNCTION_TEMPLATE_INFO_TYPE,
                           WASM_JS_FUNCTION_DATA_TYPE,
                           WASM_CAPI_FUNCTION_DATA_TYPE};
  Label check_is_bytecode_array(this);
  Label check_is_exported_function_data(this);
  Label check_is_asm_wasm_data(this);
  Label check_is_uncompiled_data_without_preparse_data(this);
  Label check_is_uncompiled_data_with_preparse_data(this);
  Label check_is_function_template_info(this);
  Label check_is_interpreter_data(this);
  Label check_is_wasm_js_function_data(this);
  Label check_is_wasm_capi_function_data(this);
  Label* case_labels[] = {&check_is_bytecode_array,
                          &check_is_exported_function_data,
                          &check_is_asm_wasm_data,
                          &check_is_uncompiled_data_without_preparse_data,
                          &check_is_uncompiled_data_with_preparse_data,
                          &check_is_function_template_info,
                          &check_is_wasm_js_function_data,
                          &check_is_wasm_capi_function_data};
  STATIC_ASSERT(arraysize(case_values) == arraysize(case_labels));
  Switch(data_type, &check_is_interpreter_data, case_values, case_labels,
         arraysize(case_labels));

  // IsBytecodeArray: Interpret bytecode
  BIND(&check_is_bytecode_array);
  sfi_code = HeapConstant(BUILTIN_CODE(isolate(), InterpreterEntryTrampoline));
  Goto(&done);

  // IsWasmExportedFunctionData: Use the wrapper code
  BIND(&check_is_exported_function_data);
  sfi_code = CAST(LoadObjectField(
      CAST(sfi_data), WasmExportedFunctionData::kWrapperCodeOffset));
  Goto(&done);

  // IsAsmWasmData: Instantiate using AsmWasmData
  BIND(&check_is_asm_wasm_data);
  sfi_code = HeapConstant(BUILTIN_CODE(isolate(), InstantiateAsmJs));
  Goto(&done);

  // IsUncompiledDataWithPreparseData | IsUncompiledDataWithoutPreparseData:
  // Compile lazy
  BIND(&check_is_uncompiled_data_with_preparse_data);
  Goto(&check_is_uncompiled_data_without_preparse_data);
  BIND(&check_is_uncompiled_data_without_preparse_data);
  sfi_code = HeapConstant(BUILTIN_CODE(isolate(), CompileLazy));
  Goto(if_compile_lazy ? if_compile_lazy : &done);

  // IsFunctionTemplateInfo: API call
  BIND(&check_is_function_template_info);
  sfi_code = HeapConstant(BUILTIN_CODE(isolate(), HandleApiCall));
  Goto(&done);

  // IsInterpreterData: Interpret bytecode
  BIND(&check_is_interpreter_data);
  // This is the default branch, so assert that we have the expected data type.
  CSA_ASSERT(this,
             Word32Equal(data_type, Int32Constant(INTERPRETER_DATA_TYPE)));
  sfi_code = CAST(LoadObjectField(
      CAST(sfi_data), InterpreterData::kInterpreterTrampolineOffset));
  Goto(&done);

  // IsWasmJSFunctionData: Use the wrapper code.
  BIND(&check_is_wasm_js_function_data);
  sfi_code = CAST(
      LoadObjectField(CAST(sfi_data), WasmJSFunctionData::kWrapperCodeOffset));
  Goto(&done);

  // IsWasmCapiFunctionData: Use the wrapper code.
  BIND(&check_is_wasm_capi_function_data);
  sfi_code = CAST(LoadObjectField(CAST(sfi_data),
                                  WasmCapiFunctionData::kWrapperCodeOffset));
  Goto(&done);

  BIND(&done);
  return sfi_code.value();
}

Node* CodeStubAssembler::AllocateFunctionWithMapAndContext(Node* map,
                                                           Node* shared_info,
                                                           Node* context) {
  CSA_SLOW_ASSERT(this, IsMap(map));

  TNode<Code> const code = GetSharedFunctionInfoCode(shared_info);

  // TODO(ishell): All the callers of this function pass map loaded from
  // Context::STRICT_FUNCTION_WITHOUT_PROTOTYPE_MAP_INDEX. So we can remove
  // map parameter.
  CSA_ASSERT(this, Word32BinaryNot(IsConstructorMap(map)));
  CSA_ASSERT(this, Word32BinaryNot(IsFunctionWithPrototypeSlotMap(map)));
  TNode<HeapObject> const fun = Allocate(JSFunction::kSizeWithoutPrototype);
  STATIC_ASSERT(JSFunction::kSizeWithoutPrototype == 7 * kTaggedSize);
  StoreMapNoWriteBarrier(fun, map);
  StoreObjectFieldRoot(fun, JSObject::kPropertiesOrHashOffset,
                       RootIndex::kEmptyFixedArray);
  StoreObjectFieldRoot(fun, JSObject::kElementsOffset,
                       RootIndex::kEmptyFixedArray);
  StoreObjectFieldRoot(fun, JSFunction::kFeedbackCellOffset,
                       RootIndex::kManyClosuresCell);
  StoreObjectFieldNoWriteBarrier(fun, JSFunction::kSharedFunctionInfoOffset,
                                 shared_info);
  StoreObjectFieldNoWriteBarrier(fun, JSFunction::kContextOffset, context);
  StoreObjectFieldNoWriteBarrier(fun, JSFunction::kCodeOffset, code);
  return fun;
}

void CodeStubAssembler::CheckPrototypeEnumCache(Node* receiver,
                                                Node* receiver_map,
                                                Label* if_fast,
                                                Label* if_slow) {
  VARIABLE(var_object, MachineRepresentation::kTagged, receiver);
  VARIABLE(var_object_map, MachineRepresentation::kTagged, receiver_map);

  Label loop(this, {&var_object, &var_object_map}), done_loop(this);
  Goto(&loop);
  BIND(&loop);
  {
    // Check that there are no elements on the current {object}.
    Label if_no_elements(this);
    Node* object = var_object.value();
    Node* object_map = var_object_map.value();

    // The following relies on the elements only aliasing with JSProxy::target,
    // which is a Javascript value and hence cannot be confused with an elements
    // backing store.
    STATIC_ASSERT(static_cast<int>(JSObject::kElementsOffset) ==
                  static_cast<int>(JSProxy::kTargetOffset));
    TNode<Object> object_elements =
        LoadObjectField(object, JSObject::kElementsOffset);
    GotoIf(IsEmptyFixedArray(object_elements), &if_no_elements);
    GotoIf(IsEmptySlowElementDictionary(object_elements), &if_no_elements);

    // It might still be an empty JSArray.
    GotoIfNot(IsJSArrayMap(object_map), if_slow);
    TNode<Number> object_length = LoadJSArrayLength(object);
    Branch(TaggedEqual(object_length, SmiConstant(0)), &if_no_elements,
           if_slow);

    // Continue with the {object}s prototype.
    BIND(&if_no_elements);
    object = LoadMapPrototype(object_map);
    GotoIf(IsNull(object), if_fast);

    // For all {object}s but the {receiver}, check that the cache is empty.
    var_object.Bind(object);
    object_map = LoadMap(object);
    var_object_map.Bind(object_map);
    TNode<WordT> object_enum_length = LoadMapEnumLength(object_map);
    Branch(WordEqual(object_enum_length, IntPtrConstant(0)), &loop, if_slow);
  }
}

Node* CodeStubAssembler::CheckEnumCache(Node* receiver, Label* if_empty,
                                        Label* if_runtime) {
  Label if_fast(this), if_cache(this), if_no_cache(this, Label::kDeferred);
  TNode<Map> receiver_map = LoadMap(receiver);

  // Check if the enum length field of the {receiver} is properly initialized,
  // indicating that there is an enum cache.
  TNode<WordT> receiver_enum_length = LoadMapEnumLength(receiver_map);
  Branch(WordEqual(receiver_enum_length,
                   IntPtrConstant(kInvalidEnumCacheSentinel)),
         &if_no_cache, &if_cache);

  BIND(&if_no_cache);
  {
    // Avoid runtime-call for empty dictionary receivers.
    GotoIfNot(IsDictionaryMap(receiver_map), if_runtime);
    TNode<NameDictionary> properties = CAST(LoadSlowProperties(receiver));
    TNode<Smi> length = GetNumberOfElements(properties);
    GotoIfNot(TaggedEqual(length, SmiConstant(0)), if_runtime);
    // Check that there are no elements on the {receiver} and its prototype
    // chain. Given that we do not create an EnumCache for dict-mode objects,
    // directly jump to {if_empty} if there are no elements and no properties
    // on the {receiver}.
    CheckPrototypeEnumCache(receiver, receiver_map, if_empty, if_runtime);
  }

  // Check that there are no elements on the fast {receiver} and its
  // prototype chain.
  BIND(&if_cache);
  CheckPrototypeEnumCache(receiver, receiver_map, &if_fast, if_runtime);

  BIND(&if_fast);
  return receiver_map;
}

TNode<Object> CodeStubAssembler::GetArgumentValue(TorqueStructArguments args,
                                                  TNode<IntPtrT> index) {
  return CodeStubArguments(this, args).GetOptionalArgumentValue(index);
}

TorqueStructArguments CodeStubAssembler::GetFrameArguments(
    TNode<RawPtrT> frame, TNode<IntPtrT> argc) {
  return CodeStubArguments(this, argc, frame, INTPTR_PARAMETERS)
      .GetTorqueArguments();
}

void CodeStubAssembler::Print(const char* s) {
  std::string formatted(s);
  formatted += "\n";
  CallRuntime(Runtime::kGlobalPrint, NoContextConstant(),
              StringConstant(formatted.c_str()));
}

void CodeStubAssembler::Print(const char* prefix, Node* tagged_value) {
  if (prefix != nullptr) {
    std::string formatted(prefix);
    formatted += ": ";
    Handle<String> string = isolate()->factory()->NewStringFromAsciiChecked(
        formatted.c_str(), AllocationType::kOld);
    CallRuntime(Runtime::kGlobalPrint, NoContextConstant(),
                HeapConstant(string));
  }
  CallRuntime(Runtime::kDebugPrint, NoContextConstant(), tagged_value);
}

void CodeStubAssembler::PerformStackCheck(TNode<Context> context) {
  Label ok(this), stack_check_interrupt(this, Label::kDeferred);

  TNode<UintPtrT> stack_limit = UncheckedCast<UintPtrT>(
      Load(MachineType::Pointer(),
           ExternalConstant(ExternalReference::address_of_jslimit(isolate()))));
  TNode<BoolT> sp_within_limit = StackPointerGreaterThan(stack_limit);

  Branch(sp_within_limit, &ok, &stack_check_interrupt);

  BIND(&stack_check_interrupt);
  CallRuntime(Runtime::kStackGuard, context);
  Goto(&ok);

  BIND(&ok);
}

void CodeStubAssembler::InitializeFunctionContext(Node* native_context,
                                                  Node* context, int slots) {
  DCHECK_GE(slots, Context::MIN_CONTEXT_SLOTS);
  StoreMapNoWriteBarrier(context, RootIndex::kFunctionContextMap);
  StoreObjectFieldNoWriteBarrier(context, FixedArray::kLengthOffset,
                                 SmiConstant(slots));

  TNode<Object> const empty_scope_info =
      LoadContextElement(native_context, Context::SCOPE_INFO_INDEX);
  StoreContextElementNoWriteBarrier(context, Context::SCOPE_INFO_INDEX,
                                    empty_scope_info);
  StoreContextElementNoWriteBarrier(context, Context::PREVIOUS_INDEX,
                                    UndefinedConstant());
  StoreContextElementNoWriteBarrier(context, Context::EXTENSION_INDEX,
                                    TheHoleConstant());
  StoreContextElementNoWriteBarrier(context, Context::NATIVE_CONTEXT_INDEX,
                                    native_context);
}

TNode<JSArray> CodeStubAssembler::ArrayCreate(TNode<Context> context,
                                              TNode<Number> length) {
  TVARIABLE(JSArray, array);
  Label allocate_js_array(this);

  Label done(this), next(this), runtime(this, Label::kDeferred);
  TNode<Smi> limit = SmiConstant(JSArray::kInitialMaxFastElementArray);
  CSA_ASSERT_BRANCH(this, [=](Label* ok, Label* not_ok) {
    BranchIfNumberRelationalComparison(Operation::kGreaterThanOrEqual, length,
                                       SmiConstant(0), ok, not_ok);
  });
  // This check also transitively covers the case where length is too big
  // to be representable by a SMI and so is not usable with
  // AllocateJSArray.
  BranchIfNumberRelationalComparison(Operation::kGreaterThanOrEqual, length,
                                     limit, &runtime, &next);

  BIND(&runtime);
  {
    TNode<NativeContext> native_context = LoadNativeContext(context);
    TNode<JSFunction> array_function =
        CAST(LoadContextElement(native_context, Context::ARRAY_FUNCTION_INDEX));
    array = CAST(CallRuntime(Runtime::kNewArray, context, array_function,
                             length, array_function, UndefinedConstant()));
    Goto(&done);
  }

  BIND(&next);
  CSA_ASSERT(this, TaggedIsSmi(length));

  TNode<Map> array_map = CAST(LoadContextElement(
      context, Context::JS_ARRAY_PACKED_SMI_ELEMENTS_MAP_INDEX));

  // TODO(delphick): Consider using
  // AllocateUninitializedJSArrayWithElements to avoid initializing an
  // array and then writing over it.
  array =
      AllocateJSArray(PACKED_SMI_ELEMENTS, array_map, length, SmiConstant(0),
                      nullptr, ParameterMode::SMI_PARAMETERS);
  Goto(&done);

  BIND(&done);
  return array.value();
}

void CodeStubAssembler::SetPropertyLength(TNode<Context> context,
                                          TNode<Object> array,
                                          TNode<Number> length) {
  Label fast(this), runtime(this), done(this);
  // There's no need to set the length, if
  // 1) the array is a fast JS array and
  // 2) the new length is equal to the old length.
  // as the set is not observable. Otherwise fall back to the run-time.

  // 1) Check that the array has fast elements.
  // TODO(delphick): Consider changing this since it does an an unnecessary
  // check for SMIs.
  // TODO(delphick): Also we could hoist this to after the array construction
  // and copy the args into array in the same way as the Array constructor.
  BranchIfFastJSArray(array, context, &fast, &runtime);

  BIND(&fast);
  {
    TNode<JSArray> fast_array = CAST(array);

    TNode<Smi> length_smi = CAST(length);
    TNode<Smi> old_length = LoadFastJSArrayLength(fast_array);
    CSA_ASSERT(this, TaggedIsPositiveSmi(old_length));

    // 2) If the created array's length matches the required length, then
    //    there's nothing else to do. Otherwise use the runtime to set the
    //    property as that will insert holes into excess elements or shrink
    //    the backing store as appropriate.
    Branch(SmiNotEqual(length_smi, old_length), &runtime, &done);
  }

  BIND(&runtime);
  {
    SetPropertyStrict(context, array, CodeStubAssembler::LengthStringConstant(),
                      length);
    Goto(&done);
  }

  BIND(&done);
}

TNode<String> CodeStubAssembler::TaggedToDirectString(TNode<Object> value,
                                                      Label* fail) {
  ToDirectStringAssembler to_direct(state(), CAST(value));
  to_direct.TryToDirect(fail);
  to_direct.PointerToData(fail);
  return CAST(value);
}

PrototypeCheckAssembler::PrototypeCheckAssembler(
    compiler::CodeAssemblerState* state, Flags flags,
    TNode<NativeContext> native_context, TNode<Map> initial_prototype_map,
    Vector<DescriptorIndexNameValue> properties)
    : CodeStubAssembler(state),
      flags_(flags),
      native_context_(native_context),
      initial_prototype_map_(initial_prototype_map),
      properties_(properties) {}

void PrototypeCheckAssembler::CheckAndBranch(TNode<HeapObject> prototype,
                                             Label* if_unmodified,
                                             Label* if_modified) {
  TNode<Map> prototype_map = LoadMap(prototype);
  TNode<DescriptorArray> descriptors = LoadMapDescriptors(prototype_map);

  // The continuation of a failed fast check: if property identity checks are
  // enabled, we continue there (since they may still classify the prototype as
  // fast), otherwise we bail out.
  Label property_identity_check(this, Label::kDeferred);
  Label* if_fast_check_failed =
      ((flags_ & kCheckPrototypePropertyIdentity) == 0)
          ? if_modified
          : &property_identity_check;

  if ((flags_ & kCheckPrototypePropertyConstness) != 0) {
    // A simple prototype map identity check. Note that map identity does not
    // guarantee unmodified properties. It does guarantee that no new properties
    // have been added, or old properties deleted.

    GotoIfNot(TaggedEqual(prototype_map, initial_prototype_map_),
              if_fast_check_failed);

    // We need to make sure that relevant properties in the prototype have
    // not been tampered with. We do this by checking that their slots
    // in the prototype's descriptor array are still marked as const.

    TNode<Uint32T> combined_details;
    for (int i = 0; i < properties_.length(); i++) {
      // Assert the descriptor index is in-bounds.
      int descriptor = properties_[i].descriptor_index;
      CSA_ASSERT(this, Int32LessThan(Int32Constant(descriptor),
                                     LoadNumberOfDescriptors(descriptors)));

      // Assert that the name is correct. This essentially checks that
      // the descriptor index corresponds to the insertion order in
      // the bootstrapper.
      CSA_ASSERT(
          this,
          TaggedEqual(LoadKeyByDescriptorEntry(descriptors, descriptor),
                      CodeAssembler::LoadRoot(properties_[i].name_root_index)));

      TNode<Uint32T> details =
          DescriptorArrayGetDetails(descriptors, Uint32Constant(descriptor));

      if (i == 0) {
        combined_details = details;
      } else {
        combined_details = Word32And(combined_details, details);
      }
    }

    TNode<Uint32T> constness =
        DecodeWord32<PropertyDetails::ConstnessField>(combined_details);

    Branch(
        Word32Equal(constness,
                    Int32Constant(static_cast<int>(PropertyConstness::kConst))),
        if_unmodified, if_fast_check_failed);
  }

  if ((flags_ & kCheckPrototypePropertyIdentity) != 0) {
    // The above checks have failed, for whatever reason (maybe the prototype
    // map has changed, or a property is no longer const). This block implements
    // a more thorough check that can also accept maps which 1. do not have the
    // initial map, 2. have mutable relevant properties, but 3. still match the
    // expected value for all relevant properties.

    BIND(&property_identity_check);

    int max_descriptor_index = -1;
    for (int i = 0; i < properties_.length(); i++) {
      max_descriptor_index =
          std::max(max_descriptor_index, properties_[i].descriptor_index);
    }

    // If the greatest descriptor index is out of bounds, the map cannot be
    // fast.
    GotoIfNot(Int32LessThan(Int32Constant(max_descriptor_index),
                            LoadNumberOfDescriptors(descriptors)),
              if_modified);

    // Logic below only handles maps with fast properties.
    GotoIfMapHasSlowProperties(prototype_map, if_modified);

    for (int i = 0; i < properties_.length(); i++) {
      const DescriptorIndexNameValue& p = properties_[i];
      const int descriptor = p.descriptor_index;

      // Check if the name is correct. This essentially checks that
      // the descriptor index corresponds to the insertion order in
      // the bootstrapper.
      GotoIfNot(TaggedEqual(LoadKeyByDescriptorEntry(descriptors, descriptor),
                            CodeAssembler::LoadRoot(p.name_root_index)),
                if_modified);

      // Finally, check whether the actual value equals the expected value.
      TNode<Uint32T> details =
          DescriptorArrayGetDetails(descriptors, Uint32Constant(descriptor));
      TVARIABLE(Uint32T, var_details, details);
      TVARIABLE(Object, var_value);

      const int key_index = DescriptorArray::ToKeyIndex(descriptor);
      LoadPropertyFromFastObject(prototype, prototype_map, descriptors,
                                 IntPtrConstant(key_index), &var_details,
                                 &var_value);

      TNode<Object> actual_value = var_value.value();
      TNode<Object> expected_value =
          LoadContextElement(native_context_, p.expected_value_context_index);
      GotoIfNot(TaggedEqual(actual_value, expected_value), if_modified);
    }

    Goto(if_unmodified);
  }
}

}  // namespace internal
}  // namespace v8
