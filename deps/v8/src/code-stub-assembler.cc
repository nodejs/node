// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "src/code-stub-assembler.h"
#include "src/code-factory.h"
#include "src/frames-inl.h"
#include "src/frames.h"

namespace v8 {
namespace internal {

using compiler::Node;
template <class T>
using TNode = compiler::TNode<T>;

CodeStubAssembler::CodeStubAssembler(compiler::CodeAssemblerState* state)
    : compiler::CodeAssembler(state) {
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

void CodeStubAssembler::Assert(const NodeGenerator& condition_body,
                               const char* message, const char* file, int line,
                               Node* extra_node1, const char* extra_node1_name,
                               Node* extra_node2, const char* extra_node2_name,
                               Node* extra_node3, const char* extra_node3_name,
                               Node* extra_node4, const char* extra_node4_name,
                               Node* extra_node5,
                               const char* extra_node5_name) {
#if defined(DEBUG)
  if (FLAG_debug_code) {
    Check(condition_body, message, file, line, extra_node1, extra_node1_name,
          extra_node2, extra_node2_name, extra_node3, extra_node3_name,
          extra_node4, extra_node4_name, extra_node5, extra_node5_name);
  }
#endif
}

#ifdef DEBUG
namespace {
void MaybePrintNodeWithName(CodeStubAssembler* csa, Node* node,
                            const char* node_name) {
  if (node != nullptr) {
    csa->CallRuntime(Runtime::kPrintWithNameForAssert, csa->SmiConstant(0),
                     csa->StringConstant(node_name), node);
  }
}
}  // namespace
#endif

void CodeStubAssembler::Check(const NodeGenerator& condition_body,
                              const char* message, const char* file, int line,
                              Node* extra_node1, const char* extra_node1_name,
                              Node* extra_node2, const char* extra_node2_name,
                              Node* extra_node3, const char* extra_node3_name,
                              Node* extra_node4, const char* extra_node4_name,
                              Node* extra_node5, const char* extra_node5_name) {
  Label ok(this);
  Label not_ok(this, Label::kDeferred);
  if (message != nullptr && FLAG_code_comments) {
    Comment("[ Assert: %s", message);
  } else {
    Comment("[ Assert");
  }
  Node* condition = condition_body();
  DCHECK_NOT_NULL(condition);
  Branch(condition, &ok, &not_ok);

  BIND(&not_ok);
  DCHECK_NOT_NULL(message);
  char chars[1024];
  Vector<char> buffer(chars);
  if (file != nullptr) {
    SNPrintF(buffer, "CSA_ASSERT failed: %s [%s:%d]\n", message, file, line);
  } else {
    SNPrintF(buffer, "CSA_ASSERT failed: %s\n", message);
  }
  Node* message_node = StringConstant(&(buffer[0]));

#ifdef DEBUG
  // Only print the extra nodes in debug builds.
  MaybePrintNodeWithName(this, extra_node1, extra_node1_name);
  MaybePrintNodeWithName(this, extra_node2, extra_node2_name);
  MaybePrintNodeWithName(this, extra_node3, extra_node3_name);
  MaybePrintNodeWithName(this, extra_node4, extra_node4_name);
  MaybePrintNodeWithName(this, extra_node5, extra_node5_name);
#endif

  DebugAbort(message_node);
  Unreachable();

  BIND(&ok);
  Comment("] Assert");
}

Node* CodeStubAssembler::Select(SloppyTNode<BoolT> condition,
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

Node* CodeStubAssembler::SelectConstant(Node* condition, Node* true_value,
                                        Node* false_value,
                                        MachineRepresentation rep) {
  return Select(condition, [=] { return true_value; },
                [=] { return false_value; }, rep);
}

Node* CodeStubAssembler::SelectInt32Constant(Node* condition, int true_value,
                                             int false_value) {
  return SelectConstant(condition, Int32Constant(true_value),
                        Int32Constant(false_value),
                        MachineRepresentation::kWord32);
}

Node* CodeStubAssembler::SelectIntPtrConstant(Node* condition, int true_value,
                                              int false_value) {
  return SelectConstant(condition, IntPtrConstant(true_value),
                        IntPtrConstant(false_value),
                        MachineType::PointerRepresentation());
}

Node* CodeStubAssembler::SelectBooleanConstant(Node* condition) {
  return SelectConstant(condition, TrueConstant(), FalseConstant(),
                        MachineRepresentation::kTagged);
}

Node* CodeStubAssembler::SelectSmiConstant(Node* condition, Smi* true_value,
                                           Smi* false_value) {
  return SelectConstant(condition, SmiConstant(true_value),
                        SmiConstant(false_value),
                        MachineRepresentation::kTaggedSigned);
}

Node* CodeStubAssembler::NoContextConstant() { return SmiConstant(0); }

#define HEAP_CONSTANT_ACCESSOR(rootIndexName, rootAccessorName, name) \
  compiler::TNode<std::remove_reference<decltype(                     \
      *std::declval<Heap>().rootAccessorName())>::type>               \
      CodeStubAssembler::name##Constant() {                           \
    return UncheckedCast<std::remove_reference<decltype(              \
        *std::declval<Heap>().rootAccessorName())>::type>(            \
        LoadRoot(Heap::k##rootIndexName##RootIndex));                 \
  }
HEAP_CONSTANT_LIST(HEAP_CONSTANT_ACCESSOR);
#undef HEAP_CONSTANT_ACCESSOR

#define HEAP_CONSTANT_TEST(rootIndexName, rootAccessorName, name) \
  compiler::TNode<BoolT> CodeStubAssembler::Is##name(             \
      SloppyTNode<Object> value) {                                \
    return WordEqual(value, name##Constant());                    \
  }                                                               \
  compiler::TNode<BoolT> CodeStubAssembler::IsNot##name(          \
      SloppyTNode<Object> value) {                                \
    return WordNotEqual(value, name##Constant());                 \
  }
HEAP_CONSTANT_LIST(HEAP_CONSTANT_TEST);
#undef HEAP_CONSTANT_TEST

Node* CodeStubAssembler::HashSeed() {
  return LoadAndUntagToWord32Root(Heap::kHashSeedRootIndex);
}

Node* CodeStubAssembler::StaleRegisterConstant() {
  return LoadRoot(Heap::kStaleRegisterRootIndex);
}

Node* CodeStubAssembler::IntPtrOrSmiConstant(int value, ParameterMode mode) {
  if (mode == SMI_PARAMETERS) {
    return SmiConstant(value);
  } else {
    DCHECK_EQ(INTPTR_PARAMETERS, mode);
    return IntPtrConstant(value);
  }
}

bool CodeStubAssembler::IsIntPtrOrSmiConstantZero(Node* test,
                                                  ParameterMode mode) {
  int32_t constant_test;
  Smi* smi_test;
  if (mode == INTPTR_PARAMETERS) {
    if (ToInt32Constant(test, constant_test) && constant_test == 0) {
      return true;
    }
  } else {
    DCHECK_EQ(mode, SMI_PARAMETERS);
    if (ToSmiConstant(test, smi_test) && smi_test->value() == 0) {
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
    if (ToInt32Constant(maybe_constant, int32_constant)) {
      *value = int32_constant;
      return true;
    }
  } else {
    DCHECK_EQ(mode, SMI_PARAMETERS);
    Smi* smi_constant;
    if (ToSmiConstant(maybe_constant, smi_constant)) {
      *value = Smi::ToInt(smi_constant);
      return true;
    }
  }
  return false;
}

Node* CodeStubAssembler::IntPtrRoundUpToPowerOfTwo32(Node* value) {
  Comment("IntPtrRoundUpToPowerOfTwo32");
  CSA_ASSERT(this, UintPtrLessThanOrEqual(value, IntPtrConstant(0x80000000u)));
  value = IntPtrSub(value, IntPtrConstant(1));
  for (int i = 1; i <= 16; i *= 2) {
    value = WordOr(value, WordShr(value, IntPtrConstant(i)));
  }
  return IntPtrAdd(value, IntPtrConstant(1));
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
  return WordEqual(
      Select(
          WordEqual(value, IntPtrConstant(0)),
          [=] { return IntPtrConstant(1); },
          [=] { return WordAnd(value, IntPtrSub(value, IntPtrConstant(1))); },
          MachineType::PointerRepresentation()),
      IntPtrConstant(0));
}

TNode<Float64T> CodeStubAssembler::Float64Round(SloppyTNode<Float64T> x) {
  Node* one = Float64Constant(1.0);
  Node* one_half = Float64Constant(0.5);

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

  Node* one = Float64Constant(1.0);
  Node* zero = Float64Constant(0.0);
  Node* two_52 = Float64Constant(4503599627370496.0E0);
  Node* minus_two_52 = Float64Constant(-4503599627370496.0E0);

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
    Node* minus_x = Float64Neg(x);
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

  Node* one = Float64Constant(1.0);
  Node* zero = Float64Constant(0.0);
  Node* two_52 = Float64Constant(4503599627370496.0E0);
  Node* minus_two_52 = Float64Constant(-4503599627370496.0E0);

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
    Node* minus_x = Float64Neg(x);
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
  Node* f = Float64Floor(x);
  Node* f_and_half = Float64Add(f, Float64Constant(0.5));

  VARIABLE(var_result, MachineRepresentation::kFloat64);
  Label return_f(this), return_f_plus_one(this), done(this);

  GotoIf(Float64LessThan(f_and_half, x), &return_f_plus_one);
  GotoIf(Float64LessThan(x, f_and_half), &return_f);
  {
    Node* f_mod_2 = Float64Mod(f, Float64Constant(2.0));
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

  Node* one = Float64Constant(1.0);
  Node* zero = Float64Constant(0.0);
  Node* two_52 = Float64Constant(4503599627370496.0E0);
  Node* minus_two_52 = Float64Constant(-4503599627370496.0E0);

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
      Node* minus_x = Float64Neg(x);
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

Node* CodeStubAssembler::SmiShiftBitsConstant() {
  return IntPtrConstant(kSmiShiftSize + kSmiTagSize);
}

TNode<Smi> CodeStubAssembler::SmiFromWord32(SloppyTNode<Int32T> value) {
  TNode<IntPtrT> value_intptr = ChangeInt32ToIntPtr(value);
  return BitcastWordToTaggedSigned(
      WordShl(value_intptr, SmiShiftBitsConstant()));
}

TNode<Smi> CodeStubAssembler::SmiTag(SloppyTNode<IntPtrT> value) {
  int32_t constant_value;
  if (ToInt32Constant(value, constant_value) && Smi::IsValid(constant_value)) {
    return SmiConstant(constant_value);
  }
  return BitcastWordToTaggedSigned(WordShl(value, SmiShiftBitsConstant()));
}

TNode<IntPtrT> CodeStubAssembler::SmiUntag(SloppyTNode<Smi> value) {
  intptr_t constant_value;
  if (ToIntPtrConstant(value, constant_value)) {
    return IntPtrConstant(constant_value >> (kSmiShiftSize + kSmiTagSize));
  }
  return UncheckedCast<IntPtrT>(
      WordSar(BitcastTaggedToWord(value), SmiShiftBitsConstant()));
}

TNode<Int32T> CodeStubAssembler::SmiToWord32(SloppyTNode<Smi> value) {
  TNode<IntPtrT> result = SmiUntag(value);
  return TruncateWordToWord32(result);
}

TNode<Float64T> CodeStubAssembler::SmiToFloat64(SloppyTNode<Smi> value) {
  return ChangeInt32ToFloat64(SmiToWord32(value));
}

TNode<Smi> CodeStubAssembler::SmiMax(SloppyTNode<Smi> a, SloppyTNode<Smi> b) {
  return SelectTaggedConstant(SmiLessThan(a, b), b, a);
}

TNode<Smi> CodeStubAssembler::SmiMin(SloppyTNode<Smi> a, SloppyTNode<Smi> b) {
  return SelectTaggedConstant(SmiLessThan(a, b), a, b);
}

TNode<Object> CodeStubAssembler::NumberMax(SloppyTNode<Object> a,
                                           SloppyTNode<Object> b) {
  // TODO(danno): This could be optimized by specifically handling smi cases.
  VARIABLE(result, MachineRepresentation::kTagged);
  Label done(this), greater_than_equal_a(this), greater_than_equal_b(this);
  GotoIfNumericGreaterThanOrEqual(a, b, &greater_than_equal_a);
  GotoIfNumericGreaterThanOrEqual(b, a, &greater_than_equal_b);
  result.Bind(NanConstant());
  Goto(&done);
  BIND(&greater_than_equal_a);
  result.Bind(a);
  Goto(&done);
  BIND(&greater_than_equal_b);
  result.Bind(b);
  Goto(&done);
  BIND(&done);
  return TNode<Object>::UncheckedCast(result.value());
}

TNode<Object> CodeStubAssembler::NumberMin(SloppyTNode<Object> a,
                                           SloppyTNode<Object> b) {
  // TODO(danno): This could be optimized by specifically handling smi cases.
  VARIABLE(result, MachineRepresentation::kTagged);
  Label done(this), greater_than_equal_a(this), greater_than_equal_b(this);
  GotoIfNumericGreaterThanOrEqual(a, b, &greater_than_equal_a);
  GotoIfNumericGreaterThanOrEqual(b, a, &greater_than_equal_b);
  result.Bind(NanConstant());
  Goto(&done);
  BIND(&greater_than_equal_a);
  result.Bind(b);
  Goto(&done);
  BIND(&greater_than_equal_b);
  result.Bind(a);
  Goto(&done);
  BIND(&done);
  return TNode<Object>::UncheckedCast(result.value());
}

Node* CodeStubAssembler::SmiMod(Node* a, Node* b) {
  VARIABLE(var_result, MachineRepresentation::kTagged);
  Label return_result(this, &var_result),
      return_minuszero(this, Label::kDeferred),
      return_nan(this, Label::kDeferred);

  // Untag {a} and {b}.
  a = SmiToWord32(a);
  b = SmiToWord32(b);

  // Return NaN if {b} is zero.
  GotoIf(Word32Equal(b, Int32Constant(0)), &return_nan);

  // Check if {a} is non-negative.
  Label if_aisnotnegative(this), if_aisnegative(this, Label::kDeferred);
  Branch(Int32LessThanOrEqual(Int32Constant(0), a), &if_aisnotnegative,
         &if_aisnegative);

  BIND(&if_aisnotnegative);
  {
    // Fast case, don't need to check any other edge cases.
    Node* r = Int32Mod(a, b);
    var_result.Bind(SmiFromWord32(r));
    Goto(&return_result);
  }

  BIND(&if_aisnegative);
  {
    if (SmiValuesAre32Bits()) {
      // Check if {a} is kMinInt and {b} is -1 (only relevant if the
      // kMinInt is actually representable as a Smi).
      Label join(this);
      GotoIfNot(Word32Equal(a, Int32Constant(kMinInt)), &join);
      GotoIf(Word32Equal(b, Int32Constant(-1)), &return_minuszero);
      Goto(&join);
      BIND(&join);
    }

    // Perform the integer modulus operation.
    Node* r = Int32Mod(a, b);

    // Check if {r} is zero, and if so return -0, because we have to
    // take the sign of the left hand side {a}, which is negative.
    GotoIf(Word32Equal(r, Int32Constant(0)), &return_minuszero);

    // The remainder {r} can be outside the valid Smi range on 32bit
    // architectures, so we cannot just say SmiFromWord32(r) here.
    var_result.Bind(ChangeInt32ToTagged(r));
    Goto(&return_result);
  }

  BIND(&return_minuszero);
  var_result.Bind(MinusZeroConstant());
  Goto(&return_result);

  BIND(&return_nan);
  var_result.Bind(NanConstant());
  Goto(&return_result);

  BIND(&return_result);
  return TNode<Object>::UncheckedCast(var_result.value());
}

Node* CodeStubAssembler::SmiMul(Node* a, Node* b) {
  VARIABLE(var_result, MachineRepresentation::kTagged);
  VARIABLE(var_lhs_float64, MachineRepresentation::kFloat64);
  VARIABLE(var_rhs_float64, MachineRepresentation::kFloat64);
  Label return_result(this, &var_result);

  // Both {a} and {b} are Smis. Convert them to integers and multiply.
  Node* lhs32 = SmiToWord32(a);
  Node* rhs32 = SmiToWord32(b);
  Node* pair = Int32MulWithOverflow(lhs32, rhs32);

  Node* overflow = Projection(1, pair);

  // Check if the multiplication overflowed.
  Label if_overflow(this, Label::kDeferred), if_notoverflow(this);
  Branch(overflow, &if_overflow, &if_notoverflow);
  BIND(&if_notoverflow);
  {
    // If the answer is zero, we may need to return -0.0, depending on the
    // input.
    Label answer_zero(this), answer_not_zero(this);
    Node* answer = Projection(0, pair);
    Node* zero = Int32Constant(0);
    Branch(Word32Equal(answer, zero), &answer_zero, &answer_not_zero);
    BIND(&answer_not_zero);
    {
      var_result.Bind(ChangeInt32ToTagged(answer));
      Goto(&return_result);
    }
    BIND(&answer_zero);
    {
      Node* or_result = Word32Or(lhs32, rhs32);
      Label if_should_be_negative_zero(this), if_should_be_zero(this);
      Branch(Int32LessThan(or_result, zero), &if_should_be_negative_zero,
             &if_should_be_zero);
      BIND(&if_should_be_negative_zero);
      {
        var_result.Bind(MinusZeroConstant());
        Goto(&return_result);
      }
      BIND(&if_should_be_zero);
      {
        var_result.Bind(SmiConstant(0));
        Goto(&return_result);
      }
    }
  }
  BIND(&if_overflow);
  {
    var_lhs_float64.Bind(SmiToFloat64(a));
    var_rhs_float64.Bind(SmiToFloat64(b));
    Node* value = Float64Mul(var_lhs_float64.value(), var_rhs_float64.value());
    Node* result = AllocateHeapNumberWithValue(value);
    var_result.Bind(result);
    Goto(&return_result);
  }

  BIND(&return_result);
  return var_result.value();
}

Node* CodeStubAssembler::TrySmiDiv(Node* dividend, Node* divisor,
                                   Label* bailout) {
  // Both {a} and {b} are Smis. Bailout to floating point division if {divisor}
  // is zero.
  GotoIf(WordEqual(divisor, SmiConstant(0)), bailout);

  // Do floating point division if {dividend} is zero and {divisor} is
  // negative.
  Label dividend_is_zero(this), dividend_is_not_zero(this);
  Branch(WordEqual(dividend, SmiConstant(0)), &dividend_is_zero,
         &dividend_is_not_zero);

  BIND(&dividend_is_zero);
  {
    GotoIf(SmiLessThan(divisor, SmiConstant(0)), bailout);
    Goto(&dividend_is_not_zero);
  }
  BIND(&dividend_is_not_zero);

  Node* untagged_divisor = SmiToWord32(divisor);
  Node* untagged_dividend = SmiToWord32(dividend);

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

  Node* untagged_result = Int32Div(untagged_dividend, untagged_divisor);
  Node* truncated = Int32Mul(untagged_result, untagged_divisor);

  // Do floating point division if the remainder is not 0.
  GotoIf(Word32NotEqual(untagged_dividend, truncated), bailout);

  return SmiFromWord32(untagged_result);
}

TNode<Int32T> CodeStubAssembler::TruncateWordToWord32(
    SloppyTNode<IntPtrT> value) {
  if (Is64()) {
    return TruncateInt64ToInt32(ReinterpretCast<Int64T>(value));
  }
  return ReinterpretCast<Int32T>(value);
}

TNode<BoolT> CodeStubAssembler::TaggedIsSmi(SloppyTNode<Object> a) {
  return WordEqual(WordAnd(BitcastTaggedToWord(a), IntPtrConstant(kSmiTagMask)),
                   IntPtrConstant(0));
}

TNode<BoolT> CodeStubAssembler::TaggedIsNotSmi(SloppyTNode<Object> a) {
  return WordNotEqual(
      WordAnd(BitcastTaggedToWord(a), IntPtrConstant(kSmiTagMask)),
      IntPtrConstant(0));
}

TNode<BoolT> CodeStubAssembler::TaggedIsPositiveSmi(SloppyTNode<Object> a) {
  return WordEqual(WordAnd(BitcastTaggedToWord(a),
                           IntPtrConstant(kSmiTagMask | kSmiSignMask)),
                   IntPtrConstant(0));
}

TNode<BoolT> CodeStubAssembler::WordIsWordAligned(SloppyTNode<WordT> word) {
  return WordEqual(IntPtrConstant(0),
                   WordAnd(word, IntPtrConstant(kPointerSize - 1)));
}

#if DEBUG
void CodeStubAssembler::Bind(Label* label, AssemblerDebugInfo debug_info) {
  CodeAssembler::Bind(label, debug_info);
}
#else
void CodeStubAssembler::Bind(Label* label) { CodeAssembler::Bind(label); }
#endif  // DEBUG

void CodeStubAssembler::BranchIfPrototypesHaveNoElements(
    Node* receiver_map, Label* definitely_no_elements,
    Label* possibly_elements) {
  CSA_SLOW_ASSERT(this, IsMap(receiver_map));
  VARIABLE(var_map, MachineRepresentation::kTagged, receiver_map);
  Label loop_body(this, &var_map);
  Node* empty_fixed_array = LoadRoot(Heap::kEmptyFixedArrayRootIndex);
  Node* empty_slow_element_dictionary =
      LoadRoot(Heap::kEmptySlowElementDictionaryRootIndex);
  Goto(&loop_body);

  BIND(&loop_body);
  {
    Node* map = var_map.value();
    Node* prototype = LoadMapPrototype(map);
    GotoIf(IsNull(prototype), definitely_no_elements);
    Node* prototype_map = LoadMap(prototype);
    Node* prototype_instance_type = LoadMapInstanceType(prototype_map);

    // Pessimistically assume elements if a Proxy, Special API Object,
    // or JSValue wrapper is found on the prototype chain. After this
    // instance type check, it's not necessary to check for interceptors or
    // access checks.
    Label if_custom(this, Label::kDeferred), if_notcustom(this);
    Branch(Int32LessThanOrEqual(prototype_instance_type,
                                Int32Constant(LAST_CUSTOM_ELEMENTS_RECEIVER)),
           &if_custom, &if_notcustom);

    BIND(&if_custom);
    {
      // For string JSValue wrappers we still support the checks as long
      // as they wrap the empty string.
      GotoIfNot(InstanceTypeEqual(prototype_instance_type, JS_VALUE_TYPE),
                possibly_elements);
      Node* prototype_value = LoadJSValueValue(prototype);
      Branch(IsEmptyString(prototype_value), &if_notcustom, possibly_elements);
    }

    BIND(&if_notcustom);
    {
      Node* prototype_elements = LoadElements(prototype);
      var_map.Bind(prototype_map);
      GotoIf(WordEqual(prototype_elements, empty_fixed_array), &loop_body);
      Branch(WordEqual(prototype_elements, empty_slow_element_dictionary),
             &loop_body, possibly_elements);
    }
  }
}

void CodeStubAssembler::BranchIfJSReceiver(Node* object, Label* if_true,
                                           Label* if_false) {
  GotoIf(TaggedIsSmi(object), if_false);
  STATIC_ASSERT(LAST_JS_RECEIVER_TYPE == LAST_TYPE);
  Branch(IsJSReceiver(object), if_true, if_false);
}

void CodeStubAssembler::BranchIfJSObject(Node* object, Label* if_true,
                                         Label* if_false) {
  GotoIf(TaggedIsSmi(object), if_false);
  STATIC_ASSERT(LAST_JS_OBJECT_TYPE == LAST_TYPE);
  Branch(IsJSObject(object), if_true, if_false);
}

TNode<BoolT> CodeStubAssembler::IsFastJSArray(SloppyTNode<Object> object,
                                              SloppyTNode<Context> context) {
  Label if_true(this), if_false(this, Label::kDeferred), exit(this);
  BranchIfFastJSArray(object, context, &if_true, &if_false);
  TVARIABLE(BoolT, var_result);
  BIND(&if_true);
  {
    var_result = ReinterpretCast<BoolT>(Int32Constant(1));
    Goto(&exit);
  }
  BIND(&if_false);
  {
    var_result = ReinterpretCast<BoolT>(Int32Constant(0));
    Goto(&exit);
  }
  BIND(&exit);
  return var_result;
}

void CodeStubAssembler::BranchIfFastJSArray(Node* object, Node* context,
                                            Label* if_true, Label* if_false) {
  GotoIfForceSlowPath(if_false);

  // Bailout if receiver is a Smi.
  GotoIf(TaggedIsSmi(object), if_false);

  Node* map = LoadMap(object);
  GotoIfNot(IsJSArrayMap(map), if_false);

  // Bailout if receiver has slow elements.
  Node* elements_kind = LoadMapElementsKind(map);
  GotoIfNot(IsFastElementsKind(elements_kind), if_false);

  // Check prototype chain if receiver does not have packed elements
  GotoIfNot(IsPrototypeInitialArrayPrototype(context, map), if_false);

  Branch(IsNoElementsProtectorCellInvalid(), if_false, if_true);
}

void CodeStubAssembler::BranchIfFastJSArrayForCopy(Node* object, Node* context,
                                                   Label* if_true,
                                                   Label* if_false) {
  GotoIf(IsSpeciesProtectorCellInvalid(), if_false);
  BranchIfFastJSArray(object, context, if_true, if_false);
}

void CodeStubAssembler::GotoIfForceSlowPath(Label* if_true) {
#if defined(DEBUG) || defined(ENABLE_FASTSLOW_SWITCH)
  Node* const force_slow_path_addr =
      ExternalConstant(ExternalReference::force_slow_path(isolate()));
  Node* const force_slow = Load(MachineType::Uint8(), force_slow_path_addr);

  GotoIf(force_slow, if_true);
#endif
}

Node* CodeStubAssembler::AllocateRaw(Node* size_in_bytes, AllocationFlags flags,
                                     Node* top_address, Node* limit_address) {
  Node* top = Load(MachineType::Pointer(), top_address);
  Node* limit = Load(MachineType::Pointer(), limit_address);

  // If there's not enough space, call the runtime.
  VARIABLE(result, MachineRepresentation::kTagged);
  Label runtime_call(this, Label::kDeferred), no_runtime_call(this);
  Label merge_runtime(this, &result);

  bool needs_double_alignment = flags & kDoubleAlignment;

  if (flags & kAllowLargeObjectAllocation) {
    Label next(this);
    GotoIf(IsRegularHeapObjectSize(size_in_bytes), &next);

    Node* runtime_flags = SmiConstant(
        Smi::FromInt(AllocateDoubleAlignFlag::encode(needs_double_alignment) |
                     AllocateTargetSpace::encode(AllocationSpace::LO_SPACE)));
    Node* const runtime_result =
        CallRuntime(Runtime::kAllocateInTargetSpace, NoContextConstant(),
                    SmiTag(size_in_bytes), runtime_flags);
    result.Bind(runtime_result);
    Goto(&merge_runtime);

    BIND(&next);
  }

  VARIABLE(adjusted_size, MachineType::PointerRepresentation(), size_in_bytes);

  if (needs_double_alignment) {
    Label not_aligned(this), done_alignment(this, &adjusted_size);

    Branch(WordAnd(top, IntPtrConstant(kDoubleAlignmentMask)), &not_aligned,
           &done_alignment);

    BIND(&not_aligned);
    Node* not_aligned_size = IntPtrAdd(size_in_bytes, IntPtrConstant(4));
    adjusted_size.Bind(not_aligned_size);
    Goto(&done_alignment);

    BIND(&done_alignment);
  }

  Node* new_top = IntPtrAdd(top, adjusted_size.value());

  Branch(UintPtrGreaterThanOrEqual(new_top, limit), &runtime_call,
         &no_runtime_call);

  BIND(&runtime_call);
  Node* runtime_result;
  if (flags & kPretenured) {
    Node* runtime_flags = SmiConstant(
        Smi::FromInt(AllocateDoubleAlignFlag::encode(needs_double_alignment) |
                     AllocateTargetSpace::encode(AllocationSpace::OLD_SPACE)));
    runtime_result =
        CallRuntime(Runtime::kAllocateInTargetSpace, NoContextConstant(),
                    SmiTag(size_in_bytes), runtime_flags);
  } else {
    runtime_result = CallRuntime(Runtime::kAllocateInNewSpace,
                                 NoContextConstant(), SmiTag(size_in_bytes));
  }
  result.Bind(runtime_result);
  Goto(&merge_runtime);

  // When there is enough space, return `top' and bump it up.
  BIND(&no_runtime_call);
  Node* no_runtime_result = top;
  StoreNoWriteBarrier(MachineType::PointerRepresentation(), top_address,
                      new_top);

  VARIABLE(address, MachineType::PointerRepresentation(), no_runtime_result);

  if (needs_double_alignment) {
    Label needs_filler(this), done_filling(this, &address);
    Branch(IntPtrEqual(adjusted_size.value(), size_in_bytes), &done_filling,
           &needs_filler);

    BIND(&needs_filler);
    // Store a filler and increase the address by kPointerSize.
    StoreNoWriteBarrier(MachineRepresentation::kTagged, top,
                        LoadRoot(Heap::kOnePointerFillerMapRootIndex));
    address.Bind(IntPtrAdd(no_runtime_result, IntPtrConstant(4)));

    Goto(&done_filling);

    BIND(&done_filling);
  }

  no_runtime_result = BitcastWordToTagged(
      IntPtrAdd(address.value(), IntPtrConstant(kHeapObjectTag)));

  result.Bind(no_runtime_result);
  Goto(&merge_runtime);

  BIND(&merge_runtime);
  return result.value();
}

Node* CodeStubAssembler::AllocateRawUnaligned(Node* size_in_bytes,
                                              AllocationFlags flags,
                                              Node* top_address,
                                              Node* limit_address) {
  DCHECK_EQ(flags & kDoubleAlignment, 0);
  return AllocateRaw(size_in_bytes, flags, top_address, limit_address);
}

Node* CodeStubAssembler::AllocateRawDoubleAligned(Node* size_in_bytes,
                                                  AllocationFlags flags,
                                                  Node* top_address,
                                                  Node* limit_address) {
#if defined(V8_HOST_ARCH_32_BIT)
  return AllocateRaw(size_in_bytes, flags | kDoubleAlignment, top_address,
                     limit_address);
#elif defined(V8_HOST_ARCH_64_BIT)
  // Allocation on 64 bit machine is naturally double aligned
  return AllocateRaw(size_in_bytes, flags & ~kDoubleAlignment, top_address,
                     limit_address);
#else
#error Architecture not supported
#endif
}

Node* CodeStubAssembler::AllocateInNewSpace(Node* size_in_bytes,
                                            AllocationFlags flags) {
  DCHECK(flags == kNone || flags == kDoubleAlignment);
  CSA_ASSERT(this, IsRegularHeapObjectSize(size_in_bytes));
  return Allocate(size_in_bytes, flags);
}

Node* CodeStubAssembler::Allocate(Node* size_in_bytes, AllocationFlags flags) {
  Comment("Allocate");
  bool const new_space = !(flags & kPretenured);
  Node* top_address = ExternalConstant(
      new_space
          ? ExternalReference::new_space_allocation_top_address(isolate())
          : ExternalReference::old_space_allocation_top_address(isolate()));
  DCHECK_EQ(kPointerSize,
            ExternalReference::new_space_allocation_limit_address(isolate())
                    .address() -
                ExternalReference::new_space_allocation_top_address(isolate())
                    .address());
  DCHECK_EQ(kPointerSize,
            ExternalReference::old_space_allocation_limit_address(isolate())
                    .address() -
                ExternalReference::old_space_allocation_top_address(isolate())
                    .address());
  Node* limit_address = IntPtrAdd(top_address, IntPtrConstant(kPointerSize));

  if (flags & kDoubleAlignment) {
    return AllocateRawDoubleAligned(size_in_bytes, flags, top_address,
                                    limit_address);
  } else {
    return AllocateRawUnaligned(size_in_bytes, flags, top_address,
                                limit_address);
  }
}

Node* CodeStubAssembler::AllocateInNewSpace(int size_in_bytes,
                                            AllocationFlags flags) {
  CHECK(flags == kNone || flags == kDoubleAlignment);
  DCHECK_LE(size_in_bytes, kMaxRegularHeapObjectSize);
  return CodeStubAssembler::Allocate(IntPtrConstant(size_in_bytes), flags);
}

Node* CodeStubAssembler::Allocate(int size_in_bytes, AllocationFlags flags) {
  return CodeStubAssembler::Allocate(IntPtrConstant(size_in_bytes), flags);
}

Node* CodeStubAssembler::InnerAllocate(Node* previous, Node* offset) {
  return BitcastWordToTagged(IntPtrAdd(BitcastTaggedToWord(previous), offset));
}

Node* CodeStubAssembler::InnerAllocate(Node* previous, int offset) {
  return InnerAllocate(previous, IntPtrConstant(offset));
}

Node* CodeStubAssembler::IsRegularHeapObjectSize(Node* size) {
  return UintPtrLessThanOrEqual(size,
                                IntPtrConstant(kMaxRegularHeapObjectSize));
}

void CodeStubAssembler::BranchIfToBooleanIsTrue(Node* value, Label* if_true,
                                                Label* if_false) {
  Label if_smi(this), if_notsmi(this), if_heapnumber(this, Label::kDeferred),
      if_bigint(this, Label::kDeferred);
  // Rule out false {value}.
  GotoIf(WordEqual(value, FalseConstant()), if_false);

  // Check if {value} is a Smi or a HeapObject.
  Branch(TaggedIsSmi(value), &if_smi, &if_notsmi);

  BIND(&if_smi);
  {
    // The {value} is a Smi, only need to check against zero.
    BranchIfSmiEqual(value, SmiConstant(0), if_false, if_true);
  }

  BIND(&if_notsmi);
  {
    // Check if {value} is the empty string.
    GotoIf(IsEmptyString(value), if_false);

    // The {value} is a HeapObject, load its map.
    Node* value_map = LoadMap(value);

    // Only null, undefined and document.all have the undetectable bit set,
    // so we can return false immediately when that bit is set.
    GotoIf(IsUndetectableMap(value_map), if_false);

    // We still need to handle numbers specially, but all other {value}s
    // that make it here yield true.
    GotoIf(IsHeapNumberMap(value_map), &if_heapnumber);
    Branch(IsBigInt(value), &if_bigint, if_true);

    BIND(&if_heapnumber);
    {
      // Load the floating point value of {value}.
      Node* value_value = LoadObjectField(value, HeapNumber::kValueOffset,
                                          MachineType::Float64());

      // Check if the floating point {value} is neither 0.0, -0.0 nor NaN.
      Branch(Float64LessThan(Float64Constant(0.0), Float64Abs(value_value)),
             if_true, if_false);
    }

    BIND(&if_bigint);
    {
      Node* result =
          CallRuntime(Runtime::kBigIntToBoolean, NoContextConstant(), value);
      CSA_ASSERT(this, IsBoolean(result));
      Branch(WordEqual(result, TrueConstant()), if_true, if_false);
    }
  }
}

Node* CodeStubAssembler::LoadFromFrame(int offset, MachineType rep) {
  Node* frame_pointer = LoadFramePointer();
  return Load(rep, frame_pointer, IntPtrConstant(offset));
}

Node* CodeStubAssembler::LoadFromParentFrame(int offset, MachineType rep) {
  Node* frame_pointer = LoadParentFramePointer();
  return Load(rep, frame_pointer, IntPtrConstant(offset));
}

Node* CodeStubAssembler::LoadBufferObject(Node* buffer, int offset,
                                          MachineType rep) {
  return Load(rep, buffer, IntPtrConstant(offset));
}

Node* CodeStubAssembler::LoadObjectField(SloppyTNode<HeapObject> object,
                                         int offset, MachineType rep) {
  return Load(rep, object, IntPtrConstant(offset - kHeapObjectTag));
}

Node* CodeStubAssembler::LoadObjectField(SloppyTNode<HeapObject> object,
                                         SloppyTNode<IntPtrT> offset,
                                         MachineType rep) {
  return Load(rep, object, IntPtrSub(offset, IntPtrConstant(kHeapObjectTag)));
}

TNode<IntPtrT> CodeStubAssembler::LoadAndUntagObjectField(
    SloppyTNode<HeapObject> object, int offset) {
  if (Is64()) {
#if V8_TARGET_LITTLE_ENDIAN
    offset += kPointerSize / 2;
#endif
    return ChangeInt32ToIntPtr(
        LoadObjectField(object, offset, MachineType::Int32()));
  } else {
    return SmiToWord(LoadObjectField(object, offset, MachineType::AnyTagged()));
  }
}

TNode<Int32T> CodeStubAssembler::LoadAndUntagToWord32ObjectField(Node* object,
                                                                 int offset) {
  if (Is64()) {
#if V8_TARGET_LITTLE_ENDIAN
    offset += kPointerSize / 2;
#endif
    return UncheckedCast<Int32T>(
        LoadObjectField(object, offset, MachineType::Int32()));
  } else {
    return SmiToWord32(
        LoadObjectField(object, offset, MachineType::AnyTagged()));
  }
}

TNode<IntPtrT> CodeStubAssembler::LoadAndUntagSmi(Node* base, int index) {
  if (Is64()) {
#if V8_TARGET_LITTLE_ENDIAN
    index += kPointerSize / 2;
#endif
    return ChangeInt32ToIntPtr(
        Load(MachineType::Int32(), base, IntPtrConstant(index)));
  } else {
    return SmiToWord(
        Load(MachineType::AnyTagged(), base, IntPtrConstant(index)));
  }
}

Node* CodeStubAssembler::LoadAndUntagToWord32Root(
    Heap::RootListIndex root_index) {
  Node* roots_array_start =
      ExternalConstant(ExternalReference::roots_array_start(isolate()));
  int index = root_index * kPointerSize;
  if (Is64()) {
#if V8_TARGET_LITTLE_ENDIAN
    index += kPointerSize / 2;
#endif
    return Load(MachineType::Int32(), roots_array_start, IntPtrConstant(index));
  } else {
    return SmiToWord32(Load(MachineType::AnyTagged(), roots_array_start,
                            IntPtrConstant(index)));
  }
}

Node* CodeStubAssembler::StoreAndTagSmi(Node* base, int offset, Node* value) {
  if (Is64()) {
    int zero_offset = offset + kPointerSize / 2;
    int payload_offset = offset;
#if V8_TARGET_LITTLE_ENDIAN
    std::swap(zero_offset, payload_offset);
#endif
    StoreNoWriteBarrier(MachineRepresentation::kWord32, base,
                        IntPtrConstant(zero_offset), Int32Constant(0));
    return StoreNoWriteBarrier(MachineRepresentation::kWord32, base,
                               IntPtrConstant(payload_offset),
                               TruncateInt64ToInt32(value));
  } else {
    return StoreNoWriteBarrier(MachineRepresentation::kTaggedSigned, base,
                               IntPtrConstant(offset), SmiTag(value));
  }
}

TNode<Float64T> CodeStubAssembler::LoadHeapNumberValue(
    SloppyTNode<HeapNumber> object) {
  return TNode<Float64T>::UncheckedCast(LoadObjectField(
      object, HeapNumber::kValueOffset, MachineType::Float64()));
}

TNode<Map> CodeStubAssembler::LoadMap(SloppyTNode<HeapObject> object) {
  return UncheckedCast<Map>(LoadObjectField(object, HeapObject::kMapOffset));
}

TNode<Int32T> CodeStubAssembler::LoadInstanceType(
    SloppyTNode<HeapObject> object) {
  return LoadMapInstanceType(LoadMap(object));
}

Node* CodeStubAssembler::HasInstanceType(Node* object,
                                         InstanceType instance_type) {
  return InstanceTypeEqual(LoadInstanceType(object), instance_type);
}

Node* CodeStubAssembler::DoesntHaveInstanceType(Node* object,
                                                InstanceType instance_type) {
  return Word32NotEqual(LoadInstanceType(object), Int32Constant(instance_type));
}

Node* CodeStubAssembler::TaggedDoesntHaveInstanceType(Node* any_tagged,
                                                      InstanceType type) {
  /* return Phi <TaggedIsSmi(val), DoesntHaveInstanceType(val, type)> */
  Node* tagged_is_smi = TaggedIsSmi(any_tagged);
  return Select(tagged_is_smi, [=]() { return tagged_is_smi; },
                [=]() { return DoesntHaveInstanceType(any_tagged, type); },
                MachineRepresentation::kBit);
}

TNode<HeapObject> CodeStubAssembler::LoadFastProperties(
    SloppyTNode<JSObject> object) {
  CSA_SLOW_ASSERT(this, Word32Not(IsDictionaryMap(LoadMap(object))));
  Node* properties = LoadObjectField(object, JSObject::kPropertiesOrHashOffset);
  return SelectTaggedConstant<HeapObject>(
      TaggedIsSmi(properties), EmptyFixedArrayConstant(), properties);
}

TNode<HeapObject> CodeStubAssembler::LoadSlowProperties(
    SloppyTNode<JSObject> object) {
  CSA_SLOW_ASSERT(this, IsDictionaryMap(LoadMap(object)));
  Node* properties = LoadObjectField(object, JSObject::kPropertiesOrHashOffset);
  return SelectTaggedConstant<HeapObject>(
      TaggedIsSmi(properties), EmptyPropertyDictionaryConstant(), properties);
}

TNode<FixedArrayBase> CodeStubAssembler::LoadElements(
    SloppyTNode<JSObject> object) {
  return CAST(LoadObjectField(object, JSObject::kElementsOffset));
}

TNode<Object> CodeStubAssembler::LoadJSArrayLength(SloppyTNode<JSArray> array) {
  CSA_ASSERT(this, IsJSArray(array));
  return CAST(LoadObjectField(array, JSArray::kLengthOffset));
}

TNode<Smi> CodeStubAssembler::LoadFastJSArrayLength(
    SloppyTNode<JSArray> array) {
  TNode<Object> length = LoadJSArrayLength(array);
  CSA_ASSERT(this, IsFastElementsKind(LoadMapElementsKind(LoadMap(array))));
  // JSArray length is always a positive Smi for fast arrays.
  CSA_SLOW_ASSERT(this, TaggedIsPositiveSmi(length));
  return UncheckedCast<Smi>(length);
}

TNode<Smi> CodeStubAssembler::LoadFixedArrayBaseLength(
    SloppyTNode<FixedArrayBase> array) {
  return CAST(LoadObjectField(array, FixedArrayBase::kLengthOffset));
}

TNode<IntPtrT> CodeStubAssembler::LoadAndUntagFixedArrayBaseLength(
    SloppyTNode<FixedArrayBase> array) {
  return LoadAndUntagObjectField(array, FixedArrayBase::kLengthOffset);
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

TNode<Int32T> CodeStubAssembler::LoadMapInstanceType(SloppyTNode<Map> map) {
  return UncheckedCast<Int32T>(
      LoadObjectField(map, Map::kInstanceTypeOffset, MachineType::Uint16()));
}

TNode<Int32T> CodeStubAssembler::LoadMapElementsKind(SloppyTNode<Map> map) {
  CSA_SLOW_ASSERT(this, IsMap(map));
  Node* bit_field2 = LoadMapBitField2(map);
  return Signed(DecodeWord32<Map::ElementsKindBits>(bit_field2));
}

TNode<DescriptorArray> CodeStubAssembler::LoadMapDescriptors(
    SloppyTNode<Map> map) {
  CSA_SLOW_ASSERT(this, IsMap(map));
  return CAST(LoadObjectField(map, Map::kDescriptorsOffset));
}

TNode<Object> CodeStubAssembler::LoadMapPrototype(SloppyTNode<Map> map) {
  CSA_SLOW_ASSERT(this, IsMap(map));
  return CAST(LoadObjectField(map, Map::kPrototypeOffset));
}

TNode<PrototypeInfo> CodeStubAssembler::LoadMapPrototypeInfo(
    SloppyTNode<Map> map, Label* if_no_proto_info) {
  CSA_ASSERT(this, IsMap(map));
  Node* prototype_info =
      LoadObjectField(map, Map::kTransitionsOrPrototypeInfoOffset);
  GotoIf(TaggedIsSmi(prototype_info), if_no_proto_info);
  GotoIfNot(WordEqual(LoadMap(prototype_info),
                      LoadRoot(Heap::kPrototypeInfoMapRootIndex)),
            if_no_proto_info);
  return CAST(prototype_info);
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
    GotoIf(TaggedIsSmi(result), &done);
    Node* is_map_type =
        InstanceTypeEqual(LoadInstanceType(CAST(result)), MAP_TYPE);
    GotoIfNot(is_map_type, &done);
    result =
        LoadObjectField(CAST(result), Map::kConstructorOrBackPointerOffset);
    Goto(&loop);
  }
  BIND(&done);
  return result;
}

Node* CodeStubAssembler::LoadMapEnumLength(SloppyTNode<Map> map) {
  CSA_SLOW_ASSERT(this, IsMap(map));
  Node* bit_field3 = LoadMapBitField3(map);
  return DecodeWordFromWord32<Map::EnumLengthBits>(bit_field3);
}

Node* CodeStubAssembler::LoadMapBackPointer(SloppyTNode<Map> map) {
  Node* object = LoadObjectField(map, Map::kConstructorOrBackPointerOffset);
  return Select(IsMap(object), [=] { return object; },
                [=] { return UndefinedConstant(); },
                MachineRepresentation::kTagged);
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
  TNode<Int32T> properties_instance_type = LoadInstanceType(properties);

  GotoIf(InstanceTypeEqual(properties_instance_type, PROPERTY_ARRAY_TYPE),
         &if_property_array);
  Branch(InstanceTypeEqual(properties_instance_type, HASH_TABLE_TYPE),
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
    var_hash = SmiUntag(
        LoadFixedArrayElement(properties, NameDictionary::kObjectHashIndex));
    Goto(&done);
  }

  BIND(&done);
  if (if_no_hash != nullptr) {
    GotoIf(
        IntPtrEqual(var_hash, IntPtrConstant(PropertyArray::kNoHashSentinel)),
        if_no_hash);
  }
  return var_hash;
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

TNode<IntPtrT> CodeStubAssembler::LoadStringLengthAsWord(
    SloppyTNode<String> object) {
  return SmiUntag(LoadStringLengthAsSmi(object));
}

TNode<Smi> CodeStubAssembler::LoadStringLengthAsSmi(
    SloppyTNode<String> object) {
  CSA_ASSERT(this, IsString(object));
  return CAST(LoadObjectField(object, String::kLengthOffset,
                              MachineType::TaggedPointer()));
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

Node* CodeStubAssembler::LoadJSValueValue(Node* object) {
  CSA_ASSERT(this, IsJSValue(object));
  return LoadObjectField(object, JSValue::kValueOffset);
}

Node* CodeStubAssembler::LoadWeakCellValueUnchecked(Node* weak_cell) {
  // TODO(ishell): fix callers.
  return LoadObjectField(weak_cell, WeakCell::kValueOffset);
}

Node* CodeStubAssembler::LoadWeakCellValue(Node* weak_cell, Label* if_cleared) {
  CSA_ASSERT(this, IsWeakCell(weak_cell));
  Node* value = LoadWeakCellValueUnchecked(weak_cell);
  if (if_cleared != nullptr) {
    GotoIf(WordEqual(value, IntPtrConstant(0)), if_cleared);
  }
  return value;
}

Node* CodeStubAssembler::LoadFixedArrayElement(Node* object, Node* index_node,
                                               int additional_offset,
                                               ParameterMode parameter_mode) {
  CSA_SLOW_ASSERT(this, IntPtrGreaterThanOrEqual(
                            ParameterToWord(index_node, parameter_mode),
                            IntPtrConstant(0)));
  int32_t header_size =
      FixedArray::kHeaderSize + additional_offset - kHeapObjectTag;
  Node* offset = ElementOffsetFromIndex(index_node, HOLEY_ELEMENTS,
                                        parameter_mode, header_size);
  return Load(MachineType::AnyTagged(), object, offset);
}

Node* CodeStubAssembler::LoadFixedTypedArrayElement(
    Node* data_pointer, Node* index_node, ElementsKind elements_kind,
    ParameterMode parameter_mode) {
  Node* offset =
      ElementOffsetFromIndex(index_node, elements_kind, parameter_mode, 0);
  MachineType type;
  switch (elements_kind) {
    case UINT8_ELEMENTS: /* fall through */
    case UINT8_CLAMPED_ELEMENTS:
      type = MachineType::Uint8();
      break;
    case INT8_ELEMENTS:
      type = MachineType::Int8();
      break;
    case UINT16_ELEMENTS:
      type = MachineType::Uint16();
      break;
    case INT16_ELEMENTS:
      type = MachineType::Int16();
      break;
    case UINT32_ELEMENTS:
      type = MachineType::Uint32();
      break;
    case INT32_ELEMENTS:
      type = MachineType::Int32();
      break;
    case FLOAT32_ELEMENTS:
      type = MachineType::Float32();
      break;
    case FLOAT64_ELEMENTS:
      type = MachineType::Float64();
      break;
    default:
      UNREACHABLE();
  }
  return Load(type, data_pointer, offset);
}

Node* CodeStubAssembler::LoadFixedTypedArrayElementAsTagged(
    Node* data_pointer, Node* index_node, ElementsKind elements_kind,
    ParameterMode parameter_mode) {
  Node* value = LoadFixedTypedArrayElement(data_pointer, index_node,
                                           elements_kind, parameter_mode);
  switch (elements_kind) {
    case ElementsKind::INT8_ELEMENTS:
    case ElementsKind::UINT8_CLAMPED_ELEMENTS:
    case ElementsKind::UINT8_ELEMENTS:
    case ElementsKind::INT16_ELEMENTS:
    case ElementsKind::UINT16_ELEMENTS:
      return SmiFromWord32(value);
    case ElementsKind::INT32_ELEMENTS:
      return ChangeInt32ToTagged(value);
    case ElementsKind::UINT32_ELEMENTS:
      return ChangeUint32ToTagged(value);
    case ElementsKind::FLOAT32_ELEMENTS:
      return AllocateHeapNumberWithValue(ChangeFloat32ToFloat64(value));
    case ElementsKind::FLOAT64_ELEMENTS:
      return AllocateHeapNumberWithValue(value);
    default:
      UNREACHABLE();
  }
}

Node* CodeStubAssembler::LoadFeedbackVectorSlot(Node* object,
                                                Node* slot_index_node,
                                                int additional_offset,
                                                ParameterMode parameter_mode) {
  CSA_SLOW_ASSERT(this, IsFeedbackVector(object));
  CSA_SLOW_ASSERT(this, MatchesParameterMode(slot_index_node, parameter_mode));
  int32_t header_size =
      FeedbackVector::kFeedbackSlotsOffset + additional_offset - kHeapObjectTag;
  Node* offset = ElementOffsetFromIndex(slot_index_node, HOLEY_ELEMENTS,
                                        parameter_mode, header_size);
  return Load(MachineType::AnyTagged(), object, offset);
}

Node* CodeStubAssembler::LoadAndUntagToWord32FixedArrayElement(
    Node* object, Node* index_node, int additional_offset,
    ParameterMode parameter_mode) {
  CSA_SLOW_ASSERT(this, IsFixedArraySubclass(object));
  CSA_SLOW_ASSERT(this, MatchesParameterMode(index_node, parameter_mode));
  int32_t header_size =
      FixedArray::kHeaderSize + additional_offset - kHeapObjectTag;
#if V8_TARGET_LITTLE_ENDIAN
  if (Is64()) {
    header_size += kPointerSize / 2;
  }
#endif
  Node* offset = ElementOffsetFromIndex(index_node, HOLEY_ELEMENTS,
                                        parameter_mode, header_size);
  if (Is64()) {
    return Load(MachineType::Int32(), object, offset);
  } else {
    return SmiToWord32(Load(MachineType::AnyTagged(), object, offset));
  }
}

Node* CodeStubAssembler::LoadFixedDoubleArrayElement(
    Node* object, Node* index_node, MachineType machine_type,
    int additional_offset, ParameterMode parameter_mode, Label* if_hole) {
  CSA_SLOW_ASSERT(this, IsFixedDoubleArray(object));
  CSA_SLOW_ASSERT(this, MatchesParameterMode(index_node, parameter_mode));
  CSA_ASSERT(this, IsFixedDoubleArray(object));
  int32_t header_size =
      FixedDoubleArray::kHeaderSize + additional_offset - kHeapObjectTag;
  Node* offset = ElementOffsetFromIndex(index_node, HOLEY_DOUBLE_ELEMENTS,
                                        parameter_mode, header_size);
  return LoadDoubleWithHoleCheck(object, offset, if_hole, machine_type);
}

Node* CodeStubAssembler::LoadDoubleWithHoleCheck(Node* base, Node* offset,
                                                 Label* if_hole,
                                                 MachineType machine_type) {
  if (if_hole) {
    // TODO(ishell): Compare only the upper part for the hole once the
    // compiler is able to fold addition of already complex |offset| with
    // |kIeeeDoubleExponentWordOffset| into one addressing mode.
    if (Is64()) {
      Node* element = Load(MachineType::Uint64(), base, offset);
      GotoIf(Word64Equal(element, Int64Constant(kHoleNanInt64)), if_hole);
    } else {
      Node* element_upper = Load(
          MachineType::Uint32(), base,
          IntPtrAdd(offset, IntPtrConstant(kIeeeDoubleExponentWordOffset)));
      GotoIf(Word32Equal(element_upper, Int32Constant(kHoleNanUpper32)),
             if_hole);
    }
  }
  if (machine_type.IsNone()) {
    // This means the actual value is not needed.
    return nullptr;
  }
  return Load(machine_type, base, offset);
}

TNode<Object> CodeStubAssembler::LoadContextElement(
    SloppyTNode<Context> context, int slot_index) {
  int offset = Context::SlotOffset(slot_index);
  return UncheckedCast<Object>(
      Load(MachineType::AnyTagged(), context, IntPtrConstant(offset)));
}

TNode<Object> CodeStubAssembler::LoadContextElement(
    SloppyTNode<Context> context, SloppyTNode<IntPtrT> slot_index) {
  Node* offset =
      IntPtrAdd(TimesPointerSize(slot_index),
                IntPtrConstant(Context::kHeaderSize - kHeapObjectTag));
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
  Node* offset =
      IntPtrAdd(TimesPointerSize(slot_index),
                IntPtrConstant(Context::kHeaderSize - kHeapObjectTag));
  Store(context, offset, value);
}

void CodeStubAssembler::StoreContextElementNoWriteBarrier(
    SloppyTNode<Context> context, int slot_index, SloppyTNode<Object> value) {
  int offset = Context::SlotOffset(slot_index);
  StoreNoWriteBarrier(MachineRepresentation::kTagged, context,
                      IntPtrConstant(offset), value);
}

TNode<Context> CodeStubAssembler::LoadNativeContext(
    SloppyTNode<Context> context) {
  return UncheckedCast<Context>(
      LoadContextElement(context, Context::NATIVE_CONTEXT_INDEX));
}

TNode<Context> CodeStubAssembler::LoadModuleContext(
    SloppyTNode<Context> context) {
  Node* module_map = LoadRoot(Heap::kModuleContextMapRootIndex);
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
    GotoIf(WordEqual(LoadMap(cur_context.value()), module_map), &context_found);

    cur_context.Bind(
        LoadContextElement(cur_context.value(), Context::PREVIOUS_INDEX));
    Goto(&context_search);
  }

  BIND(&context_found);
  return UncheckedCast<Context>(cur_context.value());
}

TNode<Map> CodeStubAssembler::LoadJSArrayElementsMap(
    SloppyTNode<Int32T> kind, SloppyTNode<Context> native_context) {
  CSA_ASSERT(this, IsFastElementsKind(kind));
  CSA_ASSERT(this, IsNativeContext(native_context));
  Node* offset = IntPtrAdd(IntPtrConstant(Context::FIRST_JS_ARRAY_MAP_SLOT),
                           ChangeInt32ToIntPtr(kind));
  return UncheckedCast<Map>(LoadContextElement(native_context, offset));
}

TNode<Map> CodeStubAssembler::LoadJSArrayElementsMap(
    ElementsKind kind, SloppyTNode<Context> native_context) {
  CSA_ASSERT(this, IsNativeContext(native_context));
  return UncheckedCast<Map>(
      LoadContextElement(native_context, Context::ArrayMapIndex(kind)));
}

Node* CodeStubAssembler::LoadJSFunctionPrototype(Node* function,
                                                 Label* if_bailout) {
  CSA_ASSERT(this, TaggedIsNotSmi(function));
  CSA_ASSERT(this, IsJSFunction(function));
  CSA_ASSERT(this, IsFunctionWithPrototypeSlotMap(LoadMap(function)));
  CSA_ASSERT(this, IsClearWord32(LoadMapBitField(LoadMap(function)),
                                 1 << Map::kHasNonInstancePrototype));
  Node* proto_or_map =
      LoadObjectField(function, JSFunction::kPrototypeOrInitialMapOffset);
  GotoIf(IsTheHole(proto_or_map), if_bailout);

  VARIABLE(var_result, MachineRepresentation::kTagged, proto_or_map);
  Label done(this, &var_result);
  GotoIfNot(IsMap(proto_or_map), &done);

  var_result.Bind(LoadMapPrototype(proto_or_map));
  Goto(&done);

  BIND(&done);
  return var_result.value();
}

void CodeStubAssembler::StoreHeapNumberValue(SloppyTNode<HeapNumber> object,
                                             SloppyTNode<Float64T> value) {
  StoreObjectFieldNoWriteBarrier(object, HeapNumber::kValueOffset, value,
                                 MachineRepresentation::kFloat64);
}

Node* CodeStubAssembler::StoreObjectField(
    Node* object, int offset, Node* value) {
  DCHECK_NE(HeapObject::kMapOffset, offset);  // Use StoreMap instead.
  return Store(object, IntPtrConstant(offset - kHeapObjectTag), value);
}

Node* CodeStubAssembler::StoreObjectField(Node* object, Node* offset,
                                          Node* value) {
  int const_offset;
  if (ToInt32Constant(offset, const_offset)) {
    return StoreObjectField(object, const_offset, value);
  }
  return Store(object, IntPtrSub(offset, IntPtrConstant(kHeapObjectTag)),
               value);
}

Node* CodeStubAssembler::StoreObjectFieldNoWriteBarrier(
    Node* object, int offset, Node* value, MachineRepresentation rep) {
  return StoreNoWriteBarrier(rep, object,
                             IntPtrConstant(offset - kHeapObjectTag), value);
}

Node* CodeStubAssembler::StoreObjectFieldNoWriteBarrier(
    Node* object, Node* offset, Node* value, MachineRepresentation rep) {
  int const_offset;
  if (ToInt32Constant(offset, const_offset)) {
    return StoreObjectFieldNoWriteBarrier(object, const_offset, value, rep);
  }
  return StoreNoWriteBarrier(
      rep, object, IntPtrSub(offset, IntPtrConstant(kHeapObjectTag)), value);
}

Node* CodeStubAssembler::StoreMap(Node* object, Node* map) {
  CSA_SLOW_ASSERT(this, IsMap(map));
  return StoreWithMapWriteBarrier(
      object, IntPtrConstant(HeapObject::kMapOffset - kHeapObjectTag), map);
}

Node* CodeStubAssembler::StoreMapNoWriteBarrier(
    Node* object, Heap::RootListIndex map_root_index) {
  return StoreMapNoWriteBarrier(object, LoadRoot(map_root_index));
}

Node* CodeStubAssembler::StoreMapNoWriteBarrier(Node* object, Node* map) {
  CSA_SLOW_ASSERT(this, IsMap(map));
  return StoreNoWriteBarrier(
      MachineRepresentation::kTagged, object,
      IntPtrConstant(HeapObject::kMapOffset - kHeapObjectTag), map);
}

Node* CodeStubAssembler::StoreObjectFieldRoot(Node* object, int offset,
                                              Heap::RootListIndex root_index) {
  if (Heap::RootIsImmortalImmovable(root_index)) {
    return StoreObjectFieldNoWriteBarrier(object, offset, LoadRoot(root_index));
  } else {
    return StoreObjectField(object, offset, LoadRoot(root_index));
  }
}

Node* CodeStubAssembler::StoreFixedArrayElement(Node* object, Node* index_node,
                                                Node* value,
                                                WriteBarrierMode barrier_mode,
                                                int additional_offset,
                                                ParameterMode parameter_mode) {
  CSA_SLOW_ASSERT(
      this, Word32Or(IsHashTable(object),
                     Word32Or(IsFixedArray(object), IsPropertyArray(object))));
  CSA_SLOW_ASSERT(this, MatchesParameterMode(index_node, parameter_mode));
  DCHECK(barrier_mode == SKIP_WRITE_BARRIER ||
         barrier_mode == UPDATE_WRITE_BARRIER);
  STATIC_ASSERT(FixedArray::kHeaderSize == PropertyArray::kHeaderSize);
  int header_size =
      FixedArray::kHeaderSize + additional_offset - kHeapObjectTag;
  Node* offset = ElementOffsetFromIndex(index_node, HOLEY_ELEMENTS,
                                        parameter_mode, header_size);
  if (barrier_mode == SKIP_WRITE_BARRIER) {
    return StoreNoWriteBarrier(MachineRepresentation::kTagged, object, offset,
                               value);
  } else {
    return Store(object, offset, value);
  }
}

Node* CodeStubAssembler::StoreFixedDoubleArrayElement(
    Node* object, Node* index_node, Node* value, ParameterMode parameter_mode) {
  CSA_ASSERT(this, IsFixedDoubleArray(object));
  CSA_SLOW_ASSERT(this, MatchesParameterMode(index_node, parameter_mode));
  Node* offset =
      ElementOffsetFromIndex(index_node, PACKED_DOUBLE_ELEMENTS, parameter_mode,
                             FixedArray::kHeaderSize - kHeapObjectTag);
  MachineRepresentation rep = MachineRepresentation::kFloat64;
  return StoreNoWriteBarrier(rep, object, offset, value);
}

Node* CodeStubAssembler::StoreFeedbackVectorSlot(Node* object,
                                                 Node* slot_index_node,
                                                 Node* value,
                                                 WriteBarrierMode barrier_mode,
                                                 int additional_offset,
                                                 ParameterMode parameter_mode) {
  CSA_SLOW_ASSERT(this, IsFeedbackVector(object));
  CSA_SLOW_ASSERT(this, MatchesParameterMode(slot_index_node, parameter_mode));
  DCHECK(barrier_mode == SKIP_WRITE_BARRIER ||
         barrier_mode == UPDATE_WRITE_BARRIER);
  int header_size =
      FeedbackVector::kFeedbackSlotsOffset + additional_offset - kHeapObjectTag;
  Node* offset = ElementOffsetFromIndex(slot_index_node, HOLEY_ELEMENTS,
                                        parameter_mode, header_size);
  if (barrier_mode == SKIP_WRITE_BARRIER) {
    return StoreNoWriteBarrier(MachineRepresentation::kTagged, object, offset,
                               value);
  } else {
    return Store(object, offset, value);
  }
}

void CodeStubAssembler::EnsureArrayLengthWritable(Node* map, Label* bailout) {
  // Check whether the length property is writable. The length property is the
  // only default named property on arrays. It's nonconfigurable, hence is
  // guaranteed to stay the first property.
  Node* descriptors = LoadMapDescriptors(map);
  Node* details =
      LoadFixedArrayElement(descriptors, DescriptorArray::ToDetailsIndex(0));
  GotoIf(IsSetSmi(details, PropertyDetails::kAttributesReadOnlyMask), bailout);
}

Node* CodeStubAssembler::EnsureArrayPushable(Node* receiver, Label* bailout) {
  // Disallow pushing onto prototypes. It might be the JSArray prototype.
  // Disallow pushing onto non-extensible objects.
  Comment("Disallow pushing onto prototypes");
  Node* map = LoadMap(receiver);
  Node* bit_field2 = LoadMapBitField2(map);
  int mask = static_cast<int>(Map::IsPrototypeMapBits::kMask) |
             (1 << Map::kIsExtensible);
  Node* test = Word32And(bit_field2, Int32Constant(mask));
  GotoIf(Word32NotEqual(test, Int32Constant(1 << Map::kIsExtensible)), bailout);

  // Disallow pushing onto arrays in dictionary named property mode. We need
  // to figure out whether the length property is still writable.
  Comment("Disallow pushing onto arrays in dictionary named property mode");
  GotoIf(IsDictionaryMap(map), bailout);

  EnsureArrayLengthWritable(map, bailout);

  Node* kind = DecodeWord32<Map::ElementsKindBits>(bit_field2);
  return kind;
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
  Comment("BuildAppendJSArray: %s", ElementsKindToString(kind));
  Label pre_bailout(this);
  Label success(this);
  TVARIABLE(Smi, var_tagged_length);
  ParameterMode mode = OptimalParameterMode();
  VARIABLE(var_length, OptimalParameterRepresentation(),
           TaggedToParameter(LoadFastJSArrayLength(array), mode));
  VARIABLE(var_elements, MachineRepresentation::kTagged, LoadElements(array));

  // Resize the capacity of the fixed array if it doesn't fit.
  TNode<IntPtrT> first = *arg_index;
  Node* growth = WordToParameter(IntPtrSub(args->GetLength(), first), mode);
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
    Node* diff = SmiSub(length, LoadFastJSArrayLength(array));
    StoreObjectFieldNoWriteBarrier(array, JSArray::kLengthOffset, length);
    *arg_index = IntPtrAdd(*arg_index, SmiUntag(diff));
    Goto(bailout);
  }

  BIND(&success);
  return var_tagged_length;
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
    Node* double_value = ChangeNumberToFloat64(value);
    StoreFixedDoubleArrayElement(elements, index,
                                 Float64SilenceNaN(double_value), mode);
  } else {
    WriteBarrierMode barrier_mode =
        IsSmiElementsKind(kind) ? SKIP_WRITE_BARRIER : UPDATE_WRITE_BARRIER;
    StoreFixedArrayElement(elements, index, value, barrier_mode, 0, mode);
  }
}

void CodeStubAssembler::BuildAppendJSArray(ElementsKind kind, Node* array,
                                           Node* value, Label* bailout) {
  CSA_SLOW_ASSERT(this, IsJSArray(array));
  Comment("BuildAppendJSArray: %s", ElementsKindToString(kind));
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

  Node* length = ParameterToTagged(var_length.value(), mode);
  StoreObjectFieldNoWriteBarrier(array, JSArray::kLengthOffset, length);
}

Node* CodeStubAssembler::AllocateCellWithValue(Node* value,
                                               WriteBarrierMode mode) {
  Node* result = Allocate(Cell::kSize, kNone);
  StoreMapNoWriteBarrier(result, Heap::kCellMapRootIndex);
  StoreCellValue(result, value, mode);
  return result;
}

Node* CodeStubAssembler::LoadCellValue(Node* cell) {
  CSA_SLOW_ASSERT(this, HasInstanceType(cell, CELL_TYPE));
  return LoadObjectField(cell, Cell::kValueOffset);
}

Node* CodeStubAssembler::StoreCellValue(Node* cell, Node* value,
                                        WriteBarrierMode mode) {
  CSA_SLOW_ASSERT(this, HasInstanceType(cell, CELL_TYPE));
  DCHECK(mode == SKIP_WRITE_BARRIER || mode == UPDATE_WRITE_BARRIER);

  if (mode == UPDATE_WRITE_BARRIER) {
    return StoreObjectField(cell, Cell::kValueOffset, value);
  } else {
    return StoreObjectFieldNoWriteBarrier(cell, Cell::kValueOffset, value);
  }
}

TNode<HeapNumber> CodeStubAssembler::AllocateHeapNumber(MutableMode mode) {
  Node* result = Allocate(HeapNumber::kSize, kNone);
  Heap::RootListIndex heap_map_index =
      mode == IMMUTABLE ? Heap::kHeapNumberMapRootIndex
                        : Heap::kMutableHeapNumberMapRootIndex;
  StoreMapNoWriteBarrier(result, heap_map_index);
  return UncheckedCast<HeapNumber>(result);
}

TNode<HeapNumber> CodeStubAssembler::AllocateHeapNumberWithValue(
    SloppyTNode<Float64T> value, MutableMode mode) {
  TNode<HeapNumber> result = AllocateHeapNumber(mode);
  StoreHeapNumberValue(result, value);
  return result;
}

Node* CodeStubAssembler::AllocateSeqOneByteString(int length,
                                                  AllocationFlags flags) {
  Comment("AllocateSeqOneByteString");
  if (length == 0) {
    return LoadRoot(Heap::kempty_stringRootIndex);
  }
  Node* result = Allocate(SeqOneByteString::SizeFor(length), flags);
  DCHECK(Heap::RootIsImmortalImmovable(Heap::kOneByteStringMapRootIndex));
  StoreMapNoWriteBarrier(result, Heap::kOneByteStringMapRootIndex);
  StoreObjectFieldNoWriteBarrier(result, SeqOneByteString::kLengthOffset,
                                 SmiConstant(length),
                                 MachineRepresentation::kTagged);
  StoreObjectFieldNoWriteBarrier(result, SeqOneByteString::kHashFieldSlot,
                                 IntPtrConstant(String::kEmptyHashField),
                                 MachineType::PointerRepresentation());
  return result;
}

Node* CodeStubAssembler::IsZeroOrFixedArray(Node* object) {
  Label out(this);
  VARIABLE(var_result, MachineRepresentation::kWord32, Int32Constant(1));

  GotoIf(WordEqual(object, SmiConstant(0)), &out);
  GotoIf(IsFixedArray(object), &out);

  var_result.Bind(Int32Constant(0));
  Goto(&out);

  BIND(&out);
  return var_result.value();
}

Node* CodeStubAssembler::AllocateSeqOneByteString(Node* context,
                                                  TNode<Smi> length,
                                                  AllocationFlags flags) {
  Comment("AllocateSeqOneByteString");
  CSA_SLOW_ASSERT(this, IsZeroOrFixedArray(context));
  VARIABLE(var_result, MachineRepresentation::kTagged);

  // Compute the SeqOneByteString size and check if it fits into new space.
  Label if_lengthiszero(this), if_sizeissmall(this),
      if_notsizeissmall(this, Label::kDeferred), if_join(this);
  GotoIf(SmiEqual(length, SmiConstant(0)), &if_lengthiszero);

  Node* raw_size = GetArrayAllocationSize(
      SmiUntag(length), UINT8_ELEMENTS, INTPTR_PARAMETERS,
      SeqOneByteString::kHeaderSize + kObjectAlignmentMask);
  Node* size = WordAnd(raw_size, IntPtrConstant(~kObjectAlignmentMask));
  Branch(IntPtrLessThanOrEqual(size, IntPtrConstant(kMaxRegularHeapObjectSize)),
         &if_sizeissmall, &if_notsizeissmall);

  BIND(&if_sizeissmall);
  {
    // Just allocate the SeqOneByteString in new space.
    Node* result = AllocateInNewSpace(size, flags);
    DCHECK(Heap::RootIsImmortalImmovable(Heap::kOneByteStringMapRootIndex));
    StoreMapNoWriteBarrier(result, Heap::kOneByteStringMapRootIndex);
    StoreObjectFieldNoWriteBarrier(result, SeqOneByteString::kLengthOffset,
                                   length, MachineRepresentation::kTagged);
    StoreObjectFieldNoWriteBarrier(result, SeqOneByteString::kHashFieldSlot,
                                   IntPtrConstant(String::kEmptyHashField),
                                   MachineType::PointerRepresentation());
    var_result.Bind(result);
    Goto(&if_join);
  }

  BIND(&if_notsizeissmall);
  {
    // We might need to allocate in large object space, go to the runtime.
    Node* result =
        CallRuntime(Runtime::kAllocateSeqOneByteString, context, length);
    var_result.Bind(result);
    Goto(&if_join);
  }

  BIND(&if_lengthiszero);
  {
    var_result.Bind(LoadRoot(Heap::kempty_stringRootIndex));
    Goto(&if_join);
  }

  BIND(&if_join);
  return var_result.value();
}

Node* CodeStubAssembler::AllocateSeqTwoByteString(int length,
                                                  AllocationFlags flags) {
  Comment("AllocateSeqTwoByteString");
  if (length == 0) {
    return LoadRoot(Heap::kempty_stringRootIndex);
  }
  Node* result = Allocate(SeqTwoByteString::SizeFor(length), flags);
  DCHECK(Heap::RootIsImmortalImmovable(Heap::kStringMapRootIndex));
  StoreMapNoWriteBarrier(result, Heap::kStringMapRootIndex);
  StoreObjectFieldNoWriteBarrier(result, SeqTwoByteString::kLengthOffset,
                                 SmiConstant(Smi::FromInt(length)),
                                 MachineRepresentation::kTagged);
  StoreObjectFieldNoWriteBarrier(result, SeqTwoByteString::kHashFieldSlot,
                                 IntPtrConstant(String::kEmptyHashField),
                                 MachineType::PointerRepresentation());
  return result;
}

Node* CodeStubAssembler::AllocateSeqTwoByteString(Node* context,
                                                  TNode<Smi> length,
                                                  AllocationFlags flags) {
  CSA_SLOW_ASSERT(this, IsFixedArray(context));
  Comment("AllocateSeqTwoByteString");
  VARIABLE(var_result, MachineRepresentation::kTagged);

  // Compute the SeqTwoByteString size and check if it fits into new space.
  Label if_lengthiszero(this), if_sizeissmall(this),
      if_notsizeissmall(this, Label::kDeferred), if_join(this);
  GotoIf(SmiEqual(length, SmiConstant(0)), &if_lengthiszero);

  Node* raw_size = GetArrayAllocationSize(
      SmiUntag(length), UINT16_ELEMENTS, INTPTR_PARAMETERS,
      SeqOneByteString::kHeaderSize + kObjectAlignmentMask);
  Node* size = WordAnd(raw_size, IntPtrConstant(~kObjectAlignmentMask));
  Branch(IntPtrLessThanOrEqual(size, IntPtrConstant(kMaxRegularHeapObjectSize)),
         &if_sizeissmall, &if_notsizeissmall);

  BIND(&if_sizeissmall);
  {
    // Just allocate the SeqTwoByteString in new space.
    Node* result = AllocateInNewSpace(size, flags);
    DCHECK(Heap::RootIsImmortalImmovable(Heap::kStringMapRootIndex));
    StoreMapNoWriteBarrier(result, Heap::kStringMapRootIndex);
    StoreObjectFieldNoWriteBarrier(result, SeqTwoByteString::kLengthOffset,
                                   length, MachineRepresentation::kTagged);
    StoreObjectFieldNoWriteBarrier(result, SeqTwoByteString::kHashFieldSlot,
                                   IntPtrConstant(String::kEmptyHashField),
                                   MachineType::PointerRepresentation());
    var_result.Bind(result);
    Goto(&if_join);
  }

  BIND(&if_notsizeissmall);
  {
    // We might need to allocate in large object space, go to the runtime.
    Node* result =
        CallRuntime(Runtime::kAllocateSeqTwoByteString, context, length);
    var_result.Bind(result);
    Goto(&if_join);
  }

  BIND(&if_lengthiszero);
  {
    var_result.Bind(LoadRoot(Heap::kempty_stringRootIndex));
    Goto(&if_join);
  }

  BIND(&if_join);
  return var_result.value();
}

Node* CodeStubAssembler::AllocateSlicedString(
    Heap::RootListIndex map_root_index, TNode<Smi> length, Node* parent,
    Node* offset) {
  CSA_ASSERT(this, IsString(parent));
  CSA_ASSERT(this, TaggedIsSmi(offset));
  Node* result = Allocate(SlicedString::kSize);
  DCHECK(Heap::RootIsImmortalImmovable(map_root_index));
  StoreMapNoWriteBarrier(result, map_root_index);
  StoreObjectFieldNoWriteBarrier(result, SlicedString::kLengthOffset, length,
                                 MachineRepresentation::kTagged);
  StoreObjectFieldNoWriteBarrier(result, SlicedString::kHashFieldSlot,
                                 IntPtrConstant(String::kEmptyHashField),
                                 MachineType::PointerRepresentation());
  StoreObjectFieldNoWriteBarrier(result, SlicedString::kParentOffset, parent,
                                 MachineRepresentation::kTagged);
  StoreObjectFieldNoWriteBarrier(result, SlicedString::kOffsetOffset, offset,
                                 MachineRepresentation::kTagged);
  return result;
}

Node* CodeStubAssembler::AllocateSlicedOneByteString(TNode<Smi> length,
                                                     Node* parent,
                                                     Node* offset) {
  return AllocateSlicedString(Heap::kSlicedOneByteStringMapRootIndex, length,
                              parent, offset);
}

Node* CodeStubAssembler::AllocateSlicedTwoByteString(TNode<Smi> length,
                                                     Node* parent,
                                                     Node* offset) {
  return AllocateSlicedString(Heap::kSlicedStringMapRootIndex, length, parent,
                              offset);
}

Node* CodeStubAssembler::AllocateConsString(Heap::RootListIndex map_root_index,
                                            TNode<Smi> length, Node* first,
                                            Node* second,
                                            AllocationFlags flags) {
  CSA_ASSERT(this, IsString(first));
  CSA_ASSERT(this, IsString(second));
  Node* result = Allocate(ConsString::kSize, flags);
  DCHECK(Heap::RootIsImmortalImmovable(map_root_index));
  StoreMapNoWriteBarrier(result, map_root_index);
  StoreObjectFieldNoWriteBarrier(result, ConsString::kLengthOffset, length,
                                 MachineRepresentation::kTagged);
  StoreObjectFieldNoWriteBarrier(result, ConsString::kHashFieldSlot,
                                 IntPtrConstant(String::kEmptyHashField),
                                 MachineType::PointerRepresentation());
  bool const new_space = !(flags & kPretenured);
  if (new_space) {
    StoreObjectFieldNoWriteBarrier(result, ConsString::kFirstOffset, first,
                                   MachineRepresentation::kTagged);
    StoreObjectFieldNoWriteBarrier(result, ConsString::kSecondOffset, second,
                                   MachineRepresentation::kTagged);
  } else {
    StoreObjectField(result, ConsString::kFirstOffset, first);
    StoreObjectField(result, ConsString::kSecondOffset, second);
  }
  return result;
}

Node* CodeStubAssembler::AllocateOneByteConsString(TNode<Smi> length,
                                                   Node* first, Node* second,
                                                   AllocationFlags flags) {
  return AllocateConsString(Heap::kConsOneByteStringMapRootIndex, length, first,
                            second, flags);
}

Node* CodeStubAssembler::AllocateTwoByteConsString(TNode<Smi> length,
                                                   Node* first, Node* second,
                                                   AllocationFlags flags) {
  return AllocateConsString(Heap::kConsStringMapRootIndex, length, first,
                            second, flags);
}

Node* CodeStubAssembler::NewConsString(Node* context, TNode<Smi> length,
                                       Node* left, Node* right,
                                       AllocationFlags flags) {
  CSA_ASSERT(this, IsFixedArray(context));
  CSA_ASSERT(this, IsString(left));
  CSA_ASSERT(this, IsString(right));
  // Added string can be a cons string.
  Comment("Allocating ConsString");
  Node* left_instance_type = LoadInstanceType(left);
  Node* right_instance_type = LoadInstanceType(right);

  // Compute intersection and difference of instance types.
  Node* anded_instance_types =
      Word32And(left_instance_type, right_instance_type);
  Node* xored_instance_types =
      Word32Xor(left_instance_type, right_instance_type);

  // We create a one-byte cons string if
  // 1. both strings are one-byte, or
  // 2. at least one of the strings is two-byte, but happens to contain only
  //    one-byte characters.
  // To do this, we check
  // 1. if both strings are one-byte, or if the one-byte data hint is set in
  //    both strings, or
  // 2. if one of the strings has the one-byte data hint set and the other
  //    string is one-byte.
  STATIC_ASSERT(kOneByteStringTag != 0);
  STATIC_ASSERT(kOneByteDataHintTag != 0);
  Label one_byte_map(this);
  Label two_byte_map(this);
  VARIABLE(result, MachineRepresentation::kTagged);
  Label done(this, &result);
  GotoIf(IsSetWord32(anded_instance_types,
                     kStringEncodingMask | kOneByteDataHintTag),
         &one_byte_map);
  Branch(Word32NotEqual(Word32And(xored_instance_types,
                                  Int32Constant(kStringEncodingMask |
                                                kOneByteDataHintMask)),
                        Int32Constant(kOneByteStringTag | kOneByteDataHintTag)),
         &two_byte_map, &one_byte_map);

  BIND(&one_byte_map);
  Comment("One-byte ConsString");
  result.Bind(AllocateOneByteConsString(length, left, right, flags));
  Goto(&done);

  BIND(&two_byte_map);
  Comment("Two-byte ConsString");
  result.Bind(AllocateTwoByteConsString(length, left, right, flags));
  Goto(&done);

  BIND(&done);

  return result.value();
}

Node* CodeStubAssembler::AllocateNameDictionary(int at_least_space_for) {
  return AllocateNameDictionary(IntPtrConstant(at_least_space_for));
}

Node* CodeStubAssembler::AllocateNameDictionary(Node* at_least_space_for) {
  CSA_ASSERT(this, UintPtrLessThanOrEqual(
                       at_least_space_for,
                       IntPtrConstant(NameDictionary::kMaxCapacity)));
  Node* capacity = HashTableComputeCapacity(at_least_space_for);
  return AllocateNameDictionaryWithCapacity(capacity);
}

Node* CodeStubAssembler::AllocateNameDictionaryWithCapacity(Node* capacity) {
  CSA_ASSERT(this, WordIsPowerOfTwo(capacity));
  CSA_ASSERT(this, IntPtrGreaterThan(capacity, IntPtrConstant(0)));
  Node* length = EntryToIndex<NameDictionary>(capacity);
  Node* store_size = IntPtrAdd(TimesPointerSize(length),
                               IntPtrConstant(NameDictionary::kHeaderSize));

  Node* result = AllocateInNewSpace(store_size);
  Comment("Initialize NameDictionary");
  // Initialize FixedArray fields.
  DCHECK(Heap::RootIsImmortalImmovable(Heap::kNameDictionaryMapRootIndex));
  StoreMapNoWriteBarrier(result, Heap::kNameDictionaryMapRootIndex);
  StoreObjectFieldNoWriteBarrier(result, FixedArray::kLengthOffset,
                                 SmiFromWord(length));
  // Initialized HashTable fields.
  Node* zero = SmiConstant(0);
  StoreFixedArrayElement(result, NameDictionary::kNumberOfElementsIndex, zero,
                         SKIP_WRITE_BARRIER);
  StoreFixedArrayElement(result, NameDictionary::kNumberOfDeletedElementsIndex,
                         zero, SKIP_WRITE_BARRIER);
  StoreFixedArrayElement(result, NameDictionary::kCapacityIndex,
                         SmiTag(capacity), SKIP_WRITE_BARRIER);
  // Initialize Dictionary fields.
  Node* filler = UndefinedConstant();
  StoreFixedArrayElement(result, NameDictionary::kNextEnumerationIndexIndex,
                         SmiConstant(PropertyDetails::kInitialIndex),
                         SKIP_WRITE_BARRIER);
  StoreFixedArrayElement(result, NameDictionary::kObjectHashIndex,
                         SmiConstant(PropertyArray::kNoHashSentinel),
                         SKIP_WRITE_BARRIER);

  // Initialize NameDictionary elements.
  Node* result_word = BitcastTaggedToWord(result);
  Node* start_address = IntPtrAdd(
      result_word, IntPtrConstant(NameDictionary::OffsetOfElementAt(
                                      NameDictionary::kElementsStartIndex) -
                                  kHeapObjectTag));
  Node* end_address = IntPtrAdd(
      result_word, IntPtrSub(store_size, IntPtrConstant(kHeapObjectTag)));
  StoreFieldsNoWriteBarrier(start_address, end_address, filler);
  return result;
}

Node* CodeStubAssembler::CopyNameDictionary(Node* dictionary,
                                            Label* large_object_fallback) {
  CSA_ASSERT(this, IsHashTable(dictionary));
  Comment("Copy boilerplate property dict");
  Node* capacity = SmiUntag(GetCapacity<NameDictionary>(dictionary));
  CSA_ASSERT(this, IntPtrGreaterThanOrEqual(capacity, IntPtrConstant(0)));
  GotoIf(UintPtrGreaterThan(
             capacity, IntPtrConstant(NameDictionary::kMaxRegularCapacity)),
         large_object_fallback);
  Node* properties = AllocateNameDictionaryWithCapacity(capacity);
  Node* length = SmiUntag(LoadFixedArrayBaseLength(dictionary));
  CopyFixedArrayElements(PACKED_ELEMENTS, dictionary, properties, length,
                         SKIP_WRITE_BARRIER, INTPTR_PARAMETERS);
  return properties;
}

Node* CodeStubAssembler::AllocateStruct(Node* map, AllocationFlags flags) {
  Comment("AllocateStruct");
  CSA_ASSERT(this, IsMap(map));
  Node* size = TimesPointerSize(LoadMapInstanceSizeInWords(map));
  Node* object = Allocate(size, flags);
  StoreMapNoWriteBarrier(object, map);
  InitializeStructBody(object, map, size, Struct::kHeaderSize);
  return object;
}

void CodeStubAssembler::InitializeStructBody(Node* object, Node* map,
                                             Node* size, int start_offset) {
  CSA_SLOW_ASSERT(this, IsMap(map));
  Comment("InitializeStructBody");
  Node* filler = UndefinedConstant();
  // Calculate the untagged field addresses.
  object = BitcastTaggedToWord(object);
  Node* start_address =
      IntPtrAdd(object, IntPtrConstant(start_offset - kHeapObjectTag));
  Node* end_address =
      IntPtrSub(IntPtrAdd(object, size), IntPtrConstant(kHeapObjectTag));
  StoreFieldsNoWriteBarrier(start_address, end_address, filler);
}

Node* CodeStubAssembler::AllocateJSObjectFromMap(
    Node* map, Node* properties, Node* elements, AllocationFlags flags,
    SlackTrackingMode slack_tracking_mode) {
  CSA_ASSERT(this, IsMap(map));
  CSA_ASSERT(this, Word32BinaryNot(IsJSFunctionMap(map)));
  CSA_ASSERT(this, Word32BinaryNot(InstanceTypeEqual(LoadMapInstanceType(map),
                                                     JS_GLOBAL_OBJECT_TYPE)));
  Node* instance_size = TimesPointerSize(LoadMapInstanceSizeInWords(map));
  Node* object = AllocateInNewSpace(instance_size, flags);
  StoreMapNoWriteBarrier(object, map);
  InitializeJSObjectFromMap(object, map, instance_size, properties, elements,
                            slack_tracking_mode);
  return object;
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
                         Heap::kEmptyFixedArrayRootIndex);
  } else {
    CSA_ASSERT(this, Word32Or(Word32Or(IsPropertyArray(properties),
                                       IsDictionary(properties)),
                              IsEmptyFixedArray(properties)));
    StoreObjectFieldNoWriteBarrier(object, JSObject::kPropertiesOrHashOffset,
                                   properties);
  }
  if (elements == nullptr) {
    StoreObjectFieldRoot(object, JSObject::kElementsOffset,
                         Heap::kEmptyFixedArrayRootIndex);
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
  CSA_ASSERT(this,
             IsClearWord32<Map::ConstructionCounter>(LoadMapBitField3(map)));
  InitializeFieldsWithRoot(object, IntPtrConstant(start_offset), instance_size,
                           Heap::kUndefinedValueRootIndex);
}

void CodeStubAssembler::InitializeJSObjectBodyWithSlackTracking(
    Node* object, Node* map, Node* instance_size) {
  CSA_SLOW_ASSERT(this, IsMap(map));
  Comment("InitializeJSObjectBodyNoSlackTracking");

  // Perform in-object slack tracking if requested.
  int start_offset = JSObject::kHeaderSize;
  Node* bit_field3 = LoadMapBitField3(map);
  Label end(this), slack_tracking(this), complete(this, Label::kDeferred);
  STATIC_ASSERT(Map::kNoSlackTracking == 0);
  GotoIf(IsSetWord32<Map::ConstructionCounter>(bit_field3), &slack_tracking);
  Comment("No slack tracking");
  InitializeJSObjectBodyNoSlackTracking(object, map, instance_size);
  Goto(&end);

  BIND(&slack_tracking);
  {
    Comment("Decrease construction counter");
    // Slack tracking is only done on initial maps.
    CSA_ASSERT(this, IsUndefined(LoadMapBackPointer(map)));
    STATIC_ASSERT(Map::ConstructionCounter::kNext == 32);
    Node* new_bit_field3 = Int32Sub(
        bit_field3, Int32Constant(1 << Map::ConstructionCounter::kShift));
    StoreObjectFieldNoWriteBarrier(map, Map::kBitField3Offset, new_bit_field3,
                                   MachineRepresentation::kWord32);
    STATIC_ASSERT(Map::kSlackTrackingCounterEnd == 1);

    // The object still has in-object slack therefore the |unsed_or_unused|
    // field contain the "used" value.
    Node* used_size = TimesPointerSize(ChangeUint32ToWord(
        LoadObjectField(map, Map::kUsedOrUnusedInstanceSizeInWordsOffset,
                        MachineType::Uint8())));

    Comment("iInitialize filler fields");
    InitializeFieldsWithRoot(object, used_size, instance_size,
                             Heap::kOnePointerFillerMapRootIndex);

    Comment("Initialize undefined fields");
    InitializeFieldsWithRoot(object, IntPtrConstant(start_offset), used_size,
                             Heap::kUndefinedValueRootIndex);

    GotoIf(IsClearWord32<Map::ConstructionCounter>(new_bit_field3), &complete);
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
  CSA_ASSERT(this, WordIsWordAligned(start_address));
  CSA_ASSERT(this, WordIsWordAligned(end_address));
  BuildFastLoop(start_address, end_address,
                [this, value](Node* current) {
                  StoreNoWriteBarrier(MachineRepresentation::kTagged, current,
                                      value);
                },
                kPointerSize, INTPTR_PARAMETERS, IndexAdvanceMode::kPost);
}

Node* CodeStubAssembler::AllocateUninitializedJSArrayWithoutElements(
    Node* array_map, Node* length, Node* allocation_site) {
  Comment("begin allocation of JSArray without elements");
  CSA_SLOW_ASSERT(this, TaggedIsPositiveSmi(length));
  CSA_SLOW_ASSERT(this, IsMap(array_map));
  int base_size = JSArray::kSize;
  if (allocation_site != nullptr) {
    base_size += AllocationMemento::kSize;
  }

  Node* size = IntPtrConstant(base_size);
  Node* array =
      AllocateUninitializedJSArray(array_map, length, allocation_site, size);
  return array;
}

std::pair<Node*, Node*>
CodeStubAssembler::AllocateUninitializedJSArrayWithElements(
    ElementsKind kind, Node* array_map, Node* length, Node* allocation_site,
    Node* capacity, ParameterMode capacity_mode) {
  Comment("begin allocation of JSArray with elements");
  CSA_SLOW_ASSERT(this, TaggedIsPositiveSmi(length));
  CSA_SLOW_ASSERT(this, IsMap(array_map));
  int base_size = JSArray::kSize;

  if (allocation_site != nullptr) {
    base_size += AllocationMemento::kSize;
  }

  int elements_offset = base_size;

  // Compute space for elements
  base_size += FixedArray::kHeaderSize;
  Node* size = ElementOffsetFromIndex(capacity, kind, capacity_mode, base_size);

  Node* array =
      AllocateUninitializedJSArray(array_map, length, allocation_site, size);

  Node* elements = InnerAllocate(array, elements_offset);
  StoreObjectFieldNoWriteBarrier(array, JSObject::kElementsOffset, elements);
  // Setup elements object.
  STATIC_ASSERT(FixedArrayBase::kHeaderSize == 2 * kPointerSize);
  Heap::RootListIndex elements_map_index =
      IsDoubleElementsKind(kind) ? Heap::kFixedDoubleArrayMapRootIndex
                                 : Heap::kFixedArrayMapRootIndex;
  DCHECK(Heap::RootIsImmortalImmovable(elements_map_index));
  StoreMapNoWriteBarrier(elements, elements_map_index);
  Node* capacity_smi = ParameterToTagged(capacity, capacity_mode);
  CSA_ASSERT(this, SmiGreaterThan(capacity_smi, SmiConstant(0)));
  StoreObjectFieldNoWriteBarrier(elements, FixedArray::kLengthOffset,
                                 capacity_smi);
  return {array, elements};
}

Node* CodeStubAssembler::AllocateUninitializedJSArray(Node* array_map,
                                                      Node* length,
                                                      Node* allocation_site,
                                                      Node* size_in_bytes) {
  CSA_SLOW_ASSERT(this, TaggedIsPositiveSmi(length));
  CSA_SLOW_ASSERT(this, IsMap(array_map));

  // Allocate space for the JSArray and the elements FixedArray in one go.
  Node* array = AllocateInNewSpace(size_in_bytes);

  Comment("write JSArray headers");
  StoreMapNoWriteBarrier(array, array_map);

  StoreObjectFieldNoWriteBarrier(array, JSArray::kLengthOffset, length);

  StoreObjectFieldRoot(array, JSArray::kPropertiesOrHashOffset,
                       Heap::kEmptyFixedArrayRootIndex);

  if (allocation_site != nullptr) {
    InitializeAllocationMemento(array, IntPtrConstant(JSArray::kSize),
                                allocation_site);
  }
  return array;
}

Node* CodeStubAssembler::AllocateJSArray(ElementsKind kind, Node* array_map,
                                         Node* capacity, Node* length,
                                         Node* allocation_site,
                                         ParameterMode capacity_mode) {
  CSA_SLOW_ASSERT(this, IsMap(array_map));
  CSA_SLOW_ASSERT(this, TaggedIsPositiveSmi(length));
  CSA_SLOW_ASSERT(this, MatchesParameterMode(capacity, capacity_mode));

  int capacity_as_constant;
  Node *array = nullptr, *elements = nullptr;
  if (IsIntPtrOrSmiConstantZero(capacity, capacity_mode)) {
    // Array is empty. Use the shared empty fixed array instead of allocating a
    // new one.
    array = AllocateUninitializedJSArrayWithoutElements(array_map, length,
                                                        allocation_site);
    StoreObjectFieldRoot(array, JSArray::kElementsOffset,
                         Heap::kEmptyFixedArrayRootIndex);
  } else if (TryGetIntPtrOrSmiConstantValue(capacity, &capacity_as_constant,
                                            capacity_mode) &&
             capacity_as_constant > 0) {
    // Allocate both array and elements object, and initialize the JSArray.
    std::tie(array, elements) = AllocateUninitializedJSArrayWithElements(
        kind, array_map, length, allocation_site, capacity, capacity_mode);
    // Fill in the elements with holes.
    FillFixedArrayWithValue(kind, elements,
                            IntPtrOrSmiConstant(0, capacity_mode), capacity,
                            Heap::kTheHoleValueRootIndex, capacity_mode);
  } else {
    Label out(this), empty(this), nonempty(this);
    VARIABLE(var_array, MachineRepresentation::kTagged);

    Branch(SmiEqual(ParameterToTagged(capacity, capacity_mode), SmiConstant(0)),
           &empty, &nonempty);

    BIND(&empty);
    {
      // Array is empty. Use the shared empty fixed array instead of allocating
      // a new one.
      var_array.Bind(AllocateUninitializedJSArrayWithoutElements(
          array_map, length, allocation_site));
      StoreObjectFieldRoot(var_array.value(), JSArray::kElementsOffset,
                           Heap::kEmptyFixedArrayRootIndex);
      Goto(&out);
    }

    BIND(&nonempty);
    {
      // Allocate both array and elements object, and initialize the JSArray.
      Node* array;
      std::tie(array, elements) = AllocateUninitializedJSArrayWithElements(
          kind, array_map, length, allocation_site, capacity, capacity_mode);
      var_array.Bind(array);
      // Fill in the elements with holes.
      FillFixedArrayWithValue(kind, elements,
                              IntPtrOrSmiConstant(0, capacity_mode), capacity,
                              Heap::kTheHoleValueRootIndex, capacity_mode);
      Goto(&out);
    }

    BIND(&out);
    array = var_array.value();
  }

  return array;
}

Node* CodeStubAssembler::ExtractFastJSArray(Node* context, Node* array,
                                            Node* begin, Node* count,
                                            ParameterMode mode, Node* capacity,
                                            Node* allocation_site) {
  Node* original_array_map = LoadMap(array);
  Node* elements_kind = LoadMapElementsKind(original_array_map);

  // Use the cannonical map for the Array's ElementsKind
  Node* native_context = LoadNativeContext(context);
  Node* array_map = LoadJSArrayElementsMap(elements_kind, native_context);

  Node* new_elements =
      ExtractFixedArray(LoadElements(array), begin, count, capacity,
                        ExtractFixedArrayFlag::kAllFixedArrays, mode);

  Node* result = AllocateUninitializedJSArrayWithoutElements(
      array_map, ParameterToTagged(count, mode), allocation_site);
  StoreObjectField(result, JSObject::kElementsOffset, new_elements);
  return result;
}

Node* CodeStubAssembler::CloneFastJSArray(Node* context, Node* array,
                                          ParameterMode mode,
                                          Node* allocation_site) {
  Node* length = LoadJSArrayLength(array);
  Node* elements = LoadElements(array);

  Node* original_array_map = LoadMap(array);
  Node* elements_kind = LoadMapElementsKind(original_array_map);

  Node* new_elements = CloneFixedArray(elements);

  // Use the cannonical map for the Array's ElementsKind
  Node* native_context = LoadNativeContext(context);
  Node* array_map = LoadJSArrayElementsMap(elements_kind, native_context);
  Node* result = AllocateUninitializedJSArrayWithoutElements(array_map, length,
                                                             allocation_site);
  StoreObjectField(result, JSObject::kElementsOffset, new_elements);
  return result;
}

Node* CodeStubAssembler::AllocateFixedArray(ElementsKind kind,
                                            Node* capacity_node,
                                            ParameterMode mode,
                                            AllocationFlags flags,
                                            Node* fixed_array_map) {
  CSA_SLOW_ASSERT(this, MatchesParameterMode(capacity_node, mode));
  CSA_ASSERT(this, IntPtrOrSmiGreaterThan(capacity_node,
                                          IntPtrOrSmiConstant(0, mode), mode));
  Node* total_size = GetFixedArrayAllocationSize(capacity_node, kind, mode);

  if (IsDoubleElementsKind(kind)) flags |= kDoubleAlignment;
  // Allocate both array and elements object, and initialize the JSArray.
  Node* array = Allocate(total_size, flags);
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
    Heap::RootListIndex map_index = IsDoubleElementsKind(kind)
                                        ? Heap::kFixedDoubleArrayMapRootIndex
                                        : Heap::kFixedArrayMapRootIndex;
    DCHECK(Heap::RootIsImmortalImmovable(map_index));
    StoreMapNoWriteBarrier(array, map_index);
  }
  StoreObjectFieldNoWriteBarrier(array, FixedArray::kLengthOffset,
                                 ParameterToTagged(capacity_node, mode));
  return array;
}

Node* CodeStubAssembler::ExtractFixedArray(Node* fixed_array, Node* first,
                                           Node* count, Node* capacity,
                                           ExtractFixedArrayFlags extract_flags,
                                           ParameterMode parameter_mode) {
  VARIABLE(var_result, MachineRepresentation::kTagged);
  VARIABLE(var_fixed_array_map, MachineRepresentation::kTagged);
  const AllocationFlags flags =
      (extract_flags & ExtractFixedArrayFlag::kNewSpaceAllocationOnly)
          ? CodeStubAssembler::kNone
          : CodeStubAssembler::kAllowLargeObjectAllocation;
  if (first == nullptr) {
    first = IntPtrOrSmiConstant(0, parameter_mode);
  }
  if (count == nullptr) {
    count =
        IntPtrOrSmiSub(TaggedToParameter(LoadFixedArrayBaseLength(fixed_array),
                                         parameter_mode),
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

  Label if_fixed_double_array(this), empty(this), cow(this),
      done(this, {&var_result, &var_fixed_array_map});
  var_fixed_array_map.Bind(LoadMap(fixed_array));
  GotoIf(WordEqual(IntPtrOrSmiConstant(0, parameter_mode), count), &empty);

  if (extract_flags & ExtractFixedArrayFlag::kFixedDoubleArrays) {
    if (extract_flags & ExtractFixedArrayFlag::kFixedArrays) {
      GotoIf(IsFixedDoubleArrayMap(var_fixed_array_map.value()),
             &if_fixed_double_array);
    } else {
      CSA_ASSERT(this, IsFixedDoubleArrayMap(var_fixed_array_map.value()));
    }
  } else {
    DCHECK(extract_flags & ExtractFixedArrayFlag::kFixedArrays);
    CSA_ASSERT(this, Word32BinaryNot(
                         IsFixedDoubleArrayMap(var_fixed_array_map.value())));
  }

  if (extract_flags & ExtractFixedArrayFlag::kFixedArrays) {
    Label new_space_check(this, {&var_fixed_array_map});
    Branch(WordEqual(var_fixed_array_map.value(),
                     LoadRoot(Heap::kFixedCOWArrayMapRootIndex)),
           &cow, &new_space_check);

    BIND(&new_space_check);

    bool handle_old_space = true;
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

    Label old_space(this, Label::kDeferred);
    if (handle_old_space) {
      GotoIfFixedArraySizeDoesntFitInNewSpace(
          capacity, &old_space, FixedArray::kHeaderSize, parameter_mode);
    }

    Comment("Copy PACKED_ELEMENTS new space");

    ElementsKind kind = PACKED_ELEMENTS;
    Node* to_elements =
        AllocateFixedArray(kind, capacity, parameter_mode,
                           AllocationFlag::kNone, var_fixed_array_map.value());
    var_result.Bind(to_elements);
    CopyFixedArrayElements(kind, fixed_array, kind, to_elements, first, count,
                           capacity, SKIP_WRITE_BARRIER, parameter_mode);
    Goto(&done);

    if (handle_old_space) {
      BIND(&old_space);
      {
        Comment("Copy PACKED_ELEMENTS old space");

        to_elements = AllocateFixedArray(kind, capacity, parameter_mode, flags,
                                         var_fixed_array_map.value());
        var_result.Bind(to_elements);
        CopyFixedArrayElements(kind, fixed_array, kind, to_elements, first,
                               count, capacity, UPDATE_WRITE_BARRIER,
                               parameter_mode);
        Goto(&done);
      }
    }

    BIND(&cow);
    {
      if (extract_flags & ExtractFixedArrayFlag::kDontCopyCOW) {
        GotoIf(WordNotEqual(IntPtrOrSmiConstant(0, parameter_mode), first),
               &new_space_check);

        var_result.Bind(fixed_array);
        Goto(&done);
      } else {
        var_fixed_array_map.Bind(LoadRoot(Heap::kFixedArrayMapRootIndex));
        Goto(&new_space_check);
      }
    }
  } else {
    Goto(&if_fixed_double_array);
  }

  if (extract_flags & ExtractFixedArrayFlag::kFixedDoubleArrays) {
    BIND(&if_fixed_double_array);

    Comment("Copy PACKED_DOUBLE_ELEMENTS");

    ElementsKind kind = PACKED_DOUBLE_ELEMENTS;
    Node* to_elements = AllocateFixedArray(kind, capacity, parameter_mode,
                                           flags, var_fixed_array_map.value());
    var_result.Bind(to_elements);
    CopyFixedArrayElements(kind, fixed_array, kind, to_elements, first, count,
                           capacity, SKIP_WRITE_BARRIER, parameter_mode);

    Goto(&done);
  }

  BIND(&empty);
  {
    Comment("Copy empty array");

    var_result.Bind(EmptyFixedArrayConstant());
    Goto(&done);
  }

  BIND(&done);
  return var_result.value();
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
  Node* total_size = GetPropertyArrayAllocationSize(capacity_node, mode);

  Node* array = Allocate(total_size, flags);
  Heap::RootListIndex map_index = Heap::kPropertyArrayMapRootIndex;
  DCHECK(Heap::RootIsImmortalImmovable(map_index));
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
  Node* value = UndefinedConstant();
  BuildFastFixedArrayForEach(array, kind, from_node, to_node,
                             [this, value](Node* array, Node* offset) {
                               StoreNoWriteBarrier(
                                   MachineRepresentation::kTagged, array,
                                   offset, value);
                             },
                             mode);
}

void CodeStubAssembler::FillFixedArrayWithValue(
    ElementsKind kind, Node* array, Node* from_node, Node* to_node,
    Heap::RootListIndex value_root_index, ParameterMode mode) {
  CSA_SLOW_ASSERT(this, MatchesParameterMode(from_node, mode));
  CSA_SLOW_ASSERT(this, MatchesParameterMode(to_node, mode));
  CSA_SLOW_ASSERT(this, IsFixedArrayWithKind(array, kind));
  DCHECK(value_root_index == Heap::kTheHoleValueRootIndex ||
         value_root_index == Heap::kUndefinedValueRootIndex);

  // Determine the value to initialize the {array} based
  // on the {value_root_index} and the elements {kind}.
  Node* value = LoadRoot(value_root_index);
  if (IsDoubleElementsKind(kind)) {
    value = LoadHeapNumberValue(value);
  }

  BuildFastFixedArrayForEach(
      array, kind, from_node, to_node,
      [this, value, kind](Node* array, Node* offset) {
        if (IsDoubleElementsKind(kind)) {
          StoreNoWriteBarrier(MachineRepresentation::kFloat64, array, offset,
                              value);
        } else {
          StoreNoWriteBarrier(MachineRepresentation::kTagged, array, offset,
                              value);
        }
      },
      mode);
}

void CodeStubAssembler::CopyFixedArrayElements(
    ElementsKind from_kind, Node* from_array, ElementsKind to_kind,
    Node* to_array, Node* first_element, Node* element_count, Node* capacity,
    WriteBarrierMode barrier_mode, ParameterMode mode) {
  CSA_SLOW_ASSERT(this, MatchesParameterMode(element_count, mode));
  CSA_SLOW_ASSERT(this, MatchesParameterMode(capacity, mode));
  CSA_SLOW_ASSERT(this, IsFixedArrayWithKindOrEmpty(from_array, from_kind));
  CSA_SLOW_ASSERT(this, IsFixedArrayWithKindOrEmpty(to_array, to_kind));
  STATIC_ASSERT(FixedArray::kHeaderSize == FixedDoubleArray::kHeaderSize);
  const int first_element_offset = FixedArray::kHeaderSize - kHeapObjectTag;
  Comment("[ CopyFixedArrayElements");

  // Typed array elements are not supported.
  DCHECK(!IsFixedTypedArrayElementsKind(from_kind));
  DCHECK(!IsFixedTypedArrayElementsKind(to_kind));

  Label done(this);
  bool from_double_elements = IsDoubleElementsKind(from_kind);
  bool to_double_elements = IsDoubleElementsKind(to_kind);
  bool doubles_to_objects_conversion =
      IsDoubleElementsKind(from_kind) && IsObjectElementsKind(to_kind);
  bool needs_write_barrier =
      doubles_to_objects_conversion ||
      (barrier_mode == UPDATE_WRITE_BARRIER && IsObjectElementsKind(to_kind));
  bool element_offset_matches =
      !needs_write_barrier && (Is64() || IsDoubleElementsKind(from_kind) ==
                                             IsDoubleElementsKind(to_kind));
  Node* double_hole =
      Is64() ? ReinterpretCast<UintPtrT>(Int64Constant(kHoleNanInt64))
             : ReinterpretCast<UintPtrT>(Int32Constant(kHoleNanLower32));

  if (doubles_to_objects_conversion) {
    // If the copy might trigger a GC, make sure that the FixedArray is
    // pre-initialized with holes to make sure that it's always in a
    // consistent state.
    FillFixedArrayWithValue(to_kind, to_array, IntPtrOrSmiConstant(0, mode),
                            capacity, Heap::kTheHoleValueRootIndex, mode);
  } else if (element_count != capacity) {
    FillFixedArrayWithValue(to_kind, to_array, element_count, capacity,
                            Heap::kTheHoleValueRootIndex, mode);
  }

  Node* first_from_element_offset =
      ElementOffsetFromIndex(first_element, from_kind, mode, 0);
  Node* limit_offset = IntPtrAdd(first_from_element_offset,
                                 IntPtrConstant(first_element_offset));
  VARIABLE(
      var_from_offset, MachineType::PointerRepresentation(),
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

  Variable* vars[] = {&var_from_offset, &var_to_offset};
  Label decrement(this, 2, vars);

  Node* to_array_adjusted =
      element_offset_matches
          ? IntPtrSub(BitcastTaggedToWord(to_array), first_from_element_offset)
          : to_array;

  Branch(WordEqual(var_from_offset.value(), limit_offset), &done, &decrement);

  BIND(&decrement);
  {
    Node* from_offset = IntPtrSub(
        var_from_offset.value(),
        IntPtrConstant(from_double_elements ? kDoubleSize : kPointerSize));
    var_from_offset.Bind(from_offset);

    Node* to_offset;
    if (element_offset_matches) {
      to_offset = from_offset;
    } else {
      to_offset = IntPtrSub(
          var_to_offset.value(),
          IntPtrConstant(to_double_elements ? kDoubleSize : kPointerSize));
      var_to_offset.Bind(to_offset);
    }

    Label next_iter(this), store_double_hole(this);
    Label* if_hole;
    if (doubles_to_objects_conversion) {
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
      StoreNoWriteBarrier(MachineRepresentation::kTagged, to_array_adjusted,
                          to_offset, value);
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
                            IntPtrAdd(to_offset, IntPtrConstant(kPointerSize)),
                            double_hole);
      }
      Goto(&next_iter);
    }

    BIND(&next_iter);
    Node* compare = WordNotEqual(from_offset, limit_offset);
    Branch(compare, &decrement, &done);
  }

  BIND(&done);
  Comment("] CopyFixedArrayElements");
}

void CodeStubAssembler::CopyPropertyArrayValues(Node* from_array,
                                                Node* to_array,
                                                Node* property_count,
                                                WriteBarrierMode barrier_mode,
                                                ParameterMode mode) {
  CSA_SLOW_ASSERT(this, MatchesParameterMode(property_count, mode));
  CSA_SLOW_ASSERT(this, Word32Or(IsPropertyArray(from_array),
                                 IsEmptyFixedArray(from_array)));
  CSA_SLOW_ASSERT(this, IsPropertyArray(to_array));
  Comment("[ CopyPropertyArrayValues");

  bool needs_write_barrier = barrier_mode == UPDATE_WRITE_BARRIER;
  Node* start = IntPtrOrSmiConstant(0, mode);
  ElementsKind kind = PACKED_ELEMENTS;
  BuildFastFixedArrayForEach(
      from_array, kind, start, property_count,
      [this, to_array, needs_write_barrier](Node* array, Node* offset) {
        Node* value = Load(MachineType::AnyTagged(), array, offset);

        if (needs_write_barrier) {
          Store(to_array, offset, value);
        } else {
          StoreNoWriteBarrier(MachineRepresentation::kTagged, to_array, offset,
                              value);
        }
      },
      mode);
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
  Comment("CopyStringCharacters %s -> %s",
          from_one_byte ? "ONE_BYTE_ENCODING" : "TWO_BYTE_ENCODING",
          to_one_byte ? "ONE_BYTE_ENCODING" : "TWO_BYTE_ENCODING");

  ElementsKind from_kind = from_one_byte ? UINT8_ELEMENTS : UINT16_ELEMENTS;
  ElementsKind to_kind = to_one_byte ? UINT8_ELEMENTS : UINT16_ELEMENTS;
  STATIC_ASSERT(SeqOneByteString::kHeaderSize == SeqTwoByteString::kHeaderSize);
  int header_size = SeqOneByteString::kHeaderSize - kHeapObjectTag;
  Node* from_offset = ElementOffsetFromIndex(from_index, from_kind,
                                             INTPTR_PARAMETERS, header_size);
  Node* to_offset =
      ElementOffsetFromIndex(to_index, to_kind, INTPTR_PARAMETERS, header_size);
  Node* byte_count =
      ElementOffsetFromIndex(character_count, from_kind, INTPTR_PARAMETERS);
  Node* limit_offset = IntPtrAdd(from_offset, byte_count);

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
                     (ToInt32Constant(from_index, from_index_constant) &&
                      ToInt32Constant(to_index, to_index_constant) &&
                      from_index_constant == to_index_constant));
  BuildFastLoop(vars, from_offset, limit_offset,
                [this, from_string, to_string, &current_to_offset, to_increment,
                 type, rep, index_same](Node* offset) {
                  Node* value = Load(type, from_string, offset);
                  StoreNoWriteBarrier(
                      rep, to_string,
                      index_same ? offset : current_to_offset.value(), value);
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
  CSA_SLOW_ASSERT(this, IsFixedArrayWithKind(array, from_kind));
  if (IsDoubleElementsKind(from_kind)) {
    Node* value =
        LoadDoubleWithHoleCheck(array, offset, if_hole, MachineType::Float64());
    if (!IsDoubleElementsKind(to_kind)) {
      value = AllocateHeapNumberWithValue(value);
    }
    return value;

  } else {
    Node* value = Load(MachineType::AnyTagged(), array, offset);
    if (if_hole) {
      GotoIf(WordEqual(value, TheHoleConstant()), if_hole);
    }
    if (IsDoubleElementsKind(to_kind)) {
      if (IsSmiElementsKind(from_kind)) {
        value = SmiToFloat64(value);
      } else {
        value = LoadHeapNumberValue(value);
      }
    }
    return value;
  }
}

Node* CodeStubAssembler::CalculateNewElementsCapacity(Node* old_capacity,
                                                      ParameterMode mode) {
  CSA_SLOW_ASSERT(this, MatchesParameterMode(old_capacity, mode));
  Node* half_old_capacity = WordOrSmiShr(old_capacity, 1, mode);
  Node* new_capacity = IntPtrOrSmiAdd(half_old_capacity, old_capacity, mode);
  Node* padding = IntPtrOrSmiConstant(16, mode);
  return IntPtrOrSmiAdd(new_capacity, padding, mode);
}

Node* CodeStubAssembler::TryGrowElementsCapacity(Node* object, Node* elements,
                                                 ElementsKind kind, Node* key,
                                                 Label* bailout) {
  CSA_SLOW_ASSERT(this, TaggedIsNotSmi(object));
  CSA_SLOW_ASSERT(this, IsFixedArrayWithKindOrEmpty(elements, kind));
  CSA_SLOW_ASSERT(this, TaggedIsSmi(key));
  Node* capacity = LoadFixedArrayBaseLength(elements);

  ParameterMode mode = OptimalParameterMode();
  capacity = TaggedToParameter(capacity, mode);
  key = TaggedToParameter(key, mode);

  return TryGrowElementsCapacity(object, elements, kind, key, capacity, mode,
                                 bailout);
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
  Node* new_elements = AllocateFixedArray(to_kind, new_capacity, mode);

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
  Node* memento = InnerAllocate(base, base_allocation_size);
  StoreMapNoWriteBarrier(memento, Heap::kAllocationMementoMapRootIndex);
  StoreObjectFieldNoWriteBarrier(
      memento, AllocationMemento::kAllocationSiteOffset, allocation_site);
  if (FLAG_allocation_site_pretenuring) {
    Node* count = LoadObjectField(allocation_site,
                                  AllocationSite::kPretenureCreateCountOffset);
    Node* incremented_count = SmiAdd(count, SmiConstant(1));
    StoreObjectFieldNoWriteBarrier(allocation_site,
                                   AllocationSite::kPretenureCreateCountOffset,
                                   incremented_count);
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
  TaggedToWord32OrBigIntImpl<Feedback::kNone, Object::Conversion::kToNumber>(
      context, value, &done, &var_result);
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
  TaggedToWord32OrBigIntImpl<Feedback::kNone, Object::Conversion::kToNumeric>(
      context, value, if_number, var_word32, if_bigint, var_bigint);
}

// Truncate {value} to word32 and jump to {if_number} if it is a Number,
// or find that it is a BigInt and jump to {if_bigint}. In either case,
// store the type feedback in {var_feedback}.
void CodeStubAssembler::TaggedToWord32OrBigIntWithFeedback(
    Node* context, Node* value, Label* if_number, Variable* var_word32,
    Label* if_bigint, Variable* var_bigint, Variable* var_feedback) {
  TaggedToWord32OrBigIntImpl<Feedback::kCollect,
                             Object::Conversion::kToNumeric>(
      context, value, if_number, var_word32, if_bigint, var_bigint,
      var_feedback);
}

template <CodeStubAssembler::Feedback feedback, Object::Conversion conversion>
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
  if (feedback == Feedback::kCollect) {
    var_feedback->Bind(SmiConstant(BinaryOperationFeedback::kNone));
  } else {
    DCHECK(var_feedback == nullptr);
  }
  Variable* loop_vars[] = {&var_value, var_feedback};
  int num_vars = feedback == Feedback::kCollect ? arraysize(loop_vars)
                                                : arraysize(loop_vars) - 1;
  Label loop(this, num_vars, loop_vars);
  Goto(&loop);
  BIND(&loop);
  {
    value = var_value.value();
    Label not_smi(this), is_heap_number(this), is_oddball(this),
        is_bigint(this);
    GotoIf(TaggedIsNotSmi(value), &not_smi);

    // {value} is a Smi.
    var_word32->Bind(SmiToWord32(value));
    if (feedback == Feedback::kCollect) {
      var_feedback->Bind(
          SmiOr(var_feedback->value(),
                SmiConstant(BinaryOperationFeedback::kSignedSmall)));
    }
    Goto(if_number);

    BIND(&not_smi);
    Node* map = LoadMap(value);
    GotoIf(IsHeapNumberMap(map), &is_heap_number);
    Node* instance_type = LoadMapInstanceType(map);
    if (conversion == Object::Conversion::kToNumeric) {
      GotoIf(IsBigIntInstanceType(instance_type), &is_bigint);
    }

    // Not HeapNumber (or BigInt if conversion == kToNumeric).
    {
      if (feedback == Feedback::kCollect) {
        // We do not require an Or with earlier feedback here because once we
        // convert the value to a Numeric, we cannot reach this path. We can
        // only reach this path on the first pass when the feedback is kNone.
        CSA_ASSERT(this, SmiEqual(var_feedback->value(),
                                  SmiConstant(BinaryOperationFeedback::kNone)));
      }
      GotoIf(InstanceTypeEqual(instance_type, ODDBALL_TYPE), &is_oddball);
      // Not an oddball either -> convert.
      auto builtin = conversion == Object::Conversion::kToNumeric
                         ? Builtins::kNonNumberToNumeric
                         : Builtins::kNonNumberToNumber;
      var_value.Bind(CallBuiltin(builtin, context, value));
      if (feedback == Feedback::kCollect) {
        var_feedback->Bind(SmiConstant(BinaryOperationFeedback::kAny));
      }
      Goto(&loop);

      BIND(&is_oddball);
      var_value.Bind(LoadObjectField(value, Oddball::kToNumberOffset));
      if (feedback == Feedback::kCollect) {
        var_feedback->Bind(
            SmiConstant(BinaryOperationFeedback::kNumberOrOddball));
      }
      Goto(&loop);
    }

    BIND(&is_heap_number);
    var_word32->Bind(TruncateHeapNumberValueToWord32(value));
    if (feedback == Feedback::kCollect) {
      var_feedback->Bind(SmiOr(var_feedback->value(),
                               SmiConstant(BinaryOperationFeedback::kNumber)));
    }
    Goto(if_number);

    if (conversion == Object::Conversion::kToNumeric) {
      BIND(&is_bigint);
      var_bigint->Bind(value);
      if (feedback == Feedback::kCollect) {
        var_feedback->Bind(
            SmiOr(var_feedback->value(),
                  SmiConstant(BinaryOperationFeedback::kBigInt)));
      }
      Goto(if_bigint);
    }
  }
}

Node* CodeStubAssembler::TruncateHeapNumberValueToWord32(Node* object) {
  Node* value = LoadHeapNumberValue(object);
  return TruncateFloat64ToWord32(value);
}

TNode<Number> CodeStubAssembler::ChangeFloat64ToTagged(
    SloppyTNode<Float64T> value) {
  TNode<Int32T> value32 = RoundFloat64ToInt32(value);
  TNode<Float64T> value64 = ChangeInt32ToFloat64(value32);

  Label if_valueisint32(this), if_valueisheapnumber(this), if_join(this);

  Label if_valueisequal(this), if_valueisnotequal(this);
  Branch(Float64Equal(value, value64), &if_valueisequal, &if_valueisnotequal);
  BIND(&if_valueisequal);
  {
    GotoIfNot(Word32Equal(value32, Int32Constant(0)), &if_valueisint32);
    Branch(Int32LessThan(UncheckedCast<Int32T>(Float64ExtractHighWord32(value)),
                         Int32Constant(0)),
           &if_valueisheapnumber, &if_valueisint32);
  }
  BIND(&if_valueisnotequal);
  Goto(&if_valueisheapnumber);

  TVARIABLE(Number, var_result);
  BIND(&if_valueisint32);
  {
    if (Is64()) {
      TNode<Smi> result = SmiTag(ChangeInt32ToIntPtr(value32));
      var_result = result;
      Goto(&if_join);
    } else {
      TNode<PairT<Int32T, BoolT>> pair = Int32AddWithOverflow(value32, value32);
      TNode<BoolT> overflow = Projection<1>(pair);
      Label if_overflow(this, Label::kDeferred), if_notoverflow(this);
      Branch(overflow, &if_overflow, &if_notoverflow);
      BIND(&if_overflow);
      Goto(&if_valueisheapnumber);
      BIND(&if_notoverflow);
      {
        TNode<IntPtrT> result = ChangeInt32ToIntPtr(Projection<0>(pair));
        var_result = BitcastWordToTaggedSigned(result);
        Goto(&if_join);
      }
    }
  }
  BIND(&if_valueisheapnumber);
  {
    var_result = AllocateHeapNumberWithValue(value);
    Goto(&if_join);
  }
  BIND(&if_join);
  return var_result;
}

TNode<Number> CodeStubAssembler::ChangeInt32ToTagged(
    SloppyTNode<Int32T> value) {
  if (Is64()) {
    return SmiTag(ChangeInt32ToIntPtr(value));
  }
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
  }
  Goto(&if_join);
  BIND(&if_notoverflow);
  {
    TNode<Smi> result =
        BitcastWordToTaggedSigned(ChangeInt32ToIntPtr(Projection<0>(pair)));
    var_result = result;
  }
  Goto(&if_join);
  BIND(&if_join);
  return var_result;
}

TNode<Number> CodeStubAssembler::ChangeUint32ToTagged(
    SloppyTNode<Uint32T> value) {
  Label if_overflow(this, Label::kDeferred), if_not_overflow(this),
      if_join(this);
  TVARIABLE(Number, var_result);
  // If {value} > 2^31 - 1, we need to store it in a HeapNumber.
  Branch(Uint32LessThan(Int32Constant(Smi::kMaxValue), value), &if_overflow,
         &if_not_overflow);

  BIND(&if_not_overflow);
  {
    if (Is64()) {
      var_result =
          SmiTag(ReinterpretCast<IntPtrT>(ChangeUint32ToUint64(value)));
    } else {
      // If tagging {value} results in an overflow, we need to use a HeapNumber
      // to represent it.
      // TODO(tebbi): This overflow can never happen.
      TNode<PairT<Int32T, BoolT>> pair = Int32AddWithOverflow(
          UncheckedCast<Int32T>(value), UncheckedCast<Int32T>(value));
      TNode<BoolT> overflow = Projection<1>(pair);
      GotoIf(overflow, &if_overflow);

      TNode<Smi> result =
          BitcastWordToTaggedSigned(ChangeInt32ToIntPtr(Projection<0>(pair)));
      var_result = result;
    }
  }
  Goto(&if_join);

  BIND(&if_overflow);
  {
    TNode<Float64T> float64_value = ChangeUint32ToFloat64(value);
    var_result = AllocateHeapNumberWithValue(float64_value);
  }
  Goto(&if_join);

  BIND(&if_join);
  return var_result;
}

TNode<String> CodeStubAssembler::ToThisString(Node* context, Node* value,
                                              char const* method_name) {
  VARIABLE(var_value, MachineRepresentation::kTagged, value);

  // Check if the {value} is a Smi or a HeapObject.
  Label if_valueissmi(this, Label::kDeferred), if_valueisnotsmi(this),
      if_valueisstring(this);
  Branch(TaggedIsSmi(value), &if_valueissmi, &if_valueisnotsmi);
  BIND(&if_valueisnotsmi);
  {
    // Load the instance type of the {value}.
    Node* value_instance_type = LoadInstanceType(value);

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
        CallRuntime(Runtime::kThrowCalledOnNullOrUndefined, context,
                    StringConstant(method_name));
        Unreachable();
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

TNode<Float64T> CodeStubAssembler::ChangeNumberToFloat64(
    SloppyTNode<Number> value) {
  // TODO(tebbi): Remove assert once argument is TNode instead of SloppyTNode.
  CSA_SLOW_ASSERT(this, IsNumber(value));
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
  return result;
}

TNode<UintPtrT> CodeStubAssembler::ChangeNonnegativeNumberToUintPtr(
    SloppyTNode<Number> value) {
  // TODO(tebbi): Remove assert once argument is TNode instead of SloppyTNode.
  CSA_SLOW_ASSERT(this, IsNumber(value));
  TVARIABLE(UintPtrT, result);
  Label smi(this), done(this, &result);
  GotoIf(TaggedIsSmi(value), &smi);

  TNode<HeapNumber> value_hn = CAST(value);
  result = ChangeFloat64ToUintPtr(LoadHeapNumberValue(value_hn));
  Goto(&done);

  BIND(&smi);
  TNode<Smi> value_smi = CAST(value);
  CSA_SLOW_ASSERT(this, SmiLessThan(SmiConstant(-1), value_smi));
  result = UncheckedCast<UintPtrT>(SmiToWord(value_smi));
  Goto(&done);

  BIND(&done);
  return result;
}

Node* CodeStubAssembler::TimesPointerSize(Node* value) {
  return WordShl(value, IntPtrConstant(kPointerSizeLog2));
}

Node* CodeStubAssembler::ToThisValue(Node* context, Node* value,
                                     PrimitiveType primitive_type,
                                     char const* method_name) {
  // We might need to loop once due to JSValue unboxing.
  VARIABLE(var_value, MachineRepresentation::kTagged, value);
  Label loop(this, &var_value), done_loop(this),
      done_throw(this, Label::kDeferred);
  Goto(&loop);
  BIND(&loop);
  {
    // Load the current {value}.
    value = var_value.value();

    // Check if the {value} is a Smi or a HeapObject.
    GotoIf(TaggedIsSmi(value), (primitive_type == PrimitiveType::kNumber)
                                   ? &done_loop
                                   : &done_throw);

    // Load the map of the {value}.
    Node* value_map = LoadMap(value);

    // Load the instance type of the {value}.
    Node* value_instance_type = LoadMapInstanceType(value_map);

    // Check if {value} is a JSValue.
    Label if_valueisvalue(this, Label::kDeferred), if_valueisnotvalue(this);
    Branch(InstanceTypeEqual(value_instance_type, JS_VALUE_TYPE),
           &if_valueisvalue, &if_valueisnotvalue);

    BIND(&if_valueisvalue);
    {
      // Load the actual value from the {value}.
      var_value.Bind(LoadObjectField(value, JSValue::kValueOffset));
      Goto(&loop);
    }

    BIND(&if_valueisnotvalue);
    {
      switch (primitive_type) {
        case PrimitiveType::kBoolean:
          GotoIf(WordEqual(value_map, BooleanMapConstant()), &done_loop);
          break;
        case PrimitiveType::kNumber:
          GotoIf(WordEqual(value_map, HeapNumberMapConstant()), &done_loop);
          break;
        case PrimitiveType::kString:
          GotoIf(IsStringInstanceType(value_instance_type), &done_loop);
          break;
        case PrimitiveType::kSymbol:
          GotoIf(WordEqual(value_map, SymbolMapConstant()), &done_loop);
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

void CodeStubAssembler::ThrowIncompatibleMethodReceiver(Node* context,
                                                        const char* method_name,
                                                        Node* receiver) {
  CallRuntime(Runtime::kThrowIncompatibleMethodReceiver, context,
              StringConstant(method_name), receiver);
  Unreachable();
}

Node* CodeStubAssembler::ThrowIfNotInstanceType(Node* context, Node* value,
                                                InstanceType instance_type,
                                                char const* method_name) {
  Label out(this), throw_exception(this, Label::kDeferred);
  VARIABLE(var_value_map, MachineRepresentation::kTagged);

  GotoIf(TaggedIsSmi(value), &throw_exception);

  // Load the instance type of the {value}.
  var_value_map.Bind(LoadMap(value));
  Node* const value_instance_type = LoadMapInstanceType(var_value_map.value());

  Branch(Word32Equal(value_instance_type, Int32Constant(instance_type)), &out,
         &throw_exception);

  // The {value} is not a compatible receiver for this method.
  BIND(&throw_exception);
  ThrowIncompatibleMethodReceiver(context, method_name, value);

  BIND(&out);
  return var_value_map.value();
}

Node* CodeStubAssembler::ThrowIfNotJSReceiver(
    Node* context, Node* value, MessageTemplate::Template msg_template,
    const char* method_name) {
  Label out(this), throw_exception(this, Label::kDeferred);
  VARIABLE(var_value_map, MachineRepresentation::kTagged);

  GotoIf(TaggedIsSmi(value), &throw_exception);

  // Load the instance type of the {value}.
  var_value_map.Bind(LoadMap(value));
  Node* const value_instance_type = LoadMapInstanceType(var_value_map.value());

  Branch(IsJSReceiverInstanceType(value_instance_type), &out, &throw_exception);

  // The {value} is not a compatible receiver for this method.
  BIND(&throw_exception);
  ThrowTypeError(context, msg_template, method_name);

  BIND(&out);
  return var_value_map.value();
}

void CodeStubAssembler::ThrowRangeError(Node* context,
                                        MessageTemplate::Template message,
                                        Node* arg0, Node* arg1, Node* arg2) {
  Node* template_index = SmiConstant(message);
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

void CodeStubAssembler::ThrowTypeError(Node* context,
                                       MessageTemplate::Template message,
                                       char const* arg0, char const* arg1) {
  Node* arg0_node = nullptr;
  if (arg0) arg0_node = StringConstant(arg0);
  Node* arg1_node = nullptr;
  if (arg1) arg1_node = StringConstant(arg1);
  ThrowTypeError(context, message, arg0_node, arg1_node);
}

void CodeStubAssembler::ThrowTypeError(Node* context,
                                       MessageTemplate::Template message,
                                       Node* arg0, Node* arg1, Node* arg2) {
  Node* template_index = SmiConstant(message);
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

Node* CodeStubAssembler::InstanceTypeEqual(Node* instance_type, int type) {
  return Word32Equal(instance_type, Int32Constant(type));
}

Node* CodeStubAssembler::IsSpecialReceiverMap(Node* map) {
  CSA_SLOW_ASSERT(this, IsMap(map));
  Node* is_special = IsSpecialReceiverInstanceType(LoadMapInstanceType(map));
  uint32_t mask =
      1 << Map::kHasNamedInterceptor | 1 << Map::kIsAccessCheckNeeded;
  USE(mask);
  // Interceptors or access checks imply special receiver.
  CSA_ASSERT(this,
             SelectConstant(IsSetWord32(LoadMapBitField(map), mask), is_special,
                            Int32Constant(1), MachineRepresentation::kWord32));
  return is_special;
}

TNode<BoolT> CodeStubAssembler::IsDictionaryMap(SloppyTNode<Map> map) {
  CSA_SLOW_ASSERT(this, IsMap(map));
  Node* bit_field3 = LoadMapBitField3(map);
  return IsSetWord32<Map::DictionaryMap>(bit_field3);
}

Node* CodeStubAssembler::IsExtensibleMap(Node* map) {
  CSA_ASSERT(this, IsMap(map));
  return IsSetWord32(LoadMapBitField2(map), 1 << Map::kIsExtensible);
}

Node* CodeStubAssembler::IsCallableMap(Node* map) {
  CSA_ASSERT(this, IsMap(map));
  return IsSetWord32(LoadMapBitField(map), 1 << Map::kIsCallable);
}

Node* CodeStubAssembler::IsDeprecatedMap(Node* map) {
  CSA_ASSERT(this, IsMap(map));
  return IsSetWord32<Map::Deprecated>(LoadMapBitField3(map));
}

Node* CodeStubAssembler::IsUndetectableMap(Node* map) {
  CSA_ASSERT(this, IsMap(map));
  return IsSetWord32(LoadMapBitField(map), 1 << Map::kIsUndetectable);
}

Node* CodeStubAssembler::IsNoElementsProtectorCellInvalid() {
  Node* invalid = SmiConstant(Isolate::kProtectorInvalid);
  Node* cell = LoadRoot(Heap::kNoElementsProtectorRootIndex);
  Node* cell_value = LoadObjectField(cell, PropertyCell::kValueOffset);
  return WordEqual(cell_value, invalid);
}

Node* CodeStubAssembler::IsSpeciesProtectorCellInvalid() {
  Node* invalid = SmiConstant(Isolate::kProtectorInvalid);
  Node* cell = LoadRoot(Heap::kSpeciesProtectorRootIndex);
  Node* cell_value = LoadObjectField(cell, PropertyCell::kValueOffset);
  return WordEqual(cell_value, invalid);
}

Node* CodeStubAssembler::IsPrototypeInitialArrayPrototype(Node* context,
                                                          Node* map) {
  Node* const native_context = LoadNativeContext(context);
  Node* const initial_array_prototype = LoadContextElement(
      native_context, Context::INITIAL_ARRAY_PROTOTYPE_INDEX);
  Node* proto = LoadMapPrototype(map);
  return WordEqual(proto, initial_array_prototype);
}

Node* CodeStubAssembler::IsCallable(Node* object) {
  return IsCallableMap(LoadMap(object));
}

Node* CodeStubAssembler::IsCell(Node* object) {
  return WordEqual(LoadMap(object), LoadRoot(Heap::kCellMapRootIndex));
}

Node* CodeStubAssembler::IsConstructorMap(Node* map) {
  CSA_ASSERT(this, IsMap(map));
  return IsSetWord32(LoadMapBitField(map), 1 << Map::kIsConstructor);
}

Node* CodeStubAssembler::IsConstructor(Node* object) {
  return IsConstructorMap(LoadMap(object));
}

Node* CodeStubAssembler::IsFunctionWithPrototypeSlotMap(Node* map) {
  CSA_ASSERT(this, IsMap(map));
  return IsSetWord32(LoadMapBitField(map), 1 << Map::kHasPrototypeSlot);
}

Node* CodeStubAssembler::IsSpecialReceiverInstanceType(Node* instance_type) {
  STATIC_ASSERT(JS_GLOBAL_OBJECT_TYPE <= LAST_SPECIAL_RECEIVER_TYPE);
  return Int32LessThanOrEqual(instance_type,
                              Int32Constant(LAST_SPECIAL_RECEIVER_TYPE));
}

Node* CodeStubAssembler::IsStringInstanceType(Node* instance_type) {
  STATIC_ASSERT(INTERNALIZED_STRING_TYPE == FIRST_TYPE);
  return Int32LessThan(instance_type, Int32Constant(FIRST_NONSTRING_TYPE));
}

Node* CodeStubAssembler::IsOneByteStringInstanceType(Node* instance_type) {
  CSA_ASSERT(this, IsStringInstanceType(instance_type));
  return Word32Equal(
      Word32And(instance_type, Int32Constant(kStringEncodingMask)),
      Int32Constant(kOneByteStringTag));
}

Node* CodeStubAssembler::IsSequentialStringInstanceType(Node* instance_type) {
  CSA_ASSERT(this, IsStringInstanceType(instance_type));
  return Word32Equal(
      Word32And(instance_type, Int32Constant(kStringRepresentationMask)),
      Int32Constant(kSeqStringTag));
}

Node* CodeStubAssembler::IsConsStringInstanceType(Node* instance_type) {
  CSA_ASSERT(this, IsStringInstanceType(instance_type));
  return Word32Equal(
      Word32And(instance_type, Int32Constant(kStringRepresentationMask)),
      Int32Constant(kConsStringTag));
}

Node* CodeStubAssembler::IsIndirectStringInstanceType(Node* instance_type) {
  CSA_ASSERT(this, IsStringInstanceType(instance_type));
  STATIC_ASSERT(kIsIndirectStringMask == 0x1);
  STATIC_ASSERT(kIsIndirectStringTag == 0x1);
  return Word32And(instance_type, Int32Constant(kIsIndirectStringMask));
}

Node* CodeStubAssembler::IsExternalStringInstanceType(Node* instance_type) {
  CSA_ASSERT(this, IsStringInstanceType(instance_type));
  return Word32Equal(
      Word32And(instance_type, Int32Constant(kStringRepresentationMask)),
      Int32Constant(kExternalStringTag));
}

Node* CodeStubAssembler::IsShortExternalStringInstanceType(
    Node* instance_type) {
  CSA_ASSERT(this, IsStringInstanceType(instance_type));
  STATIC_ASSERT(kShortExternalStringTag != 0);
  return IsSetWord32(instance_type, kShortExternalStringMask);
}

Node* CodeStubAssembler::IsJSReceiverInstanceType(Node* instance_type) {
  STATIC_ASSERT(LAST_JS_RECEIVER_TYPE == LAST_TYPE);
  return Int32GreaterThanOrEqual(instance_type,
                                 Int32Constant(FIRST_JS_RECEIVER_TYPE));
}

Node* CodeStubAssembler::IsArrayIteratorInstanceType(Node* instance_type) {
  return Uint32LessThan(
      Int32Constant(LAST_ARRAY_ITERATOR_TYPE - FIRST_ARRAY_ITERATOR_TYPE),
      Int32Sub(instance_type, Int32Constant(FIRST_ARRAY_ITERATOR_TYPE)));
}

Node* CodeStubAssembler::IsJSReceiverMap(Node* map) {
  return IsJSReceiverInstanceType(LoadMapInstanceType(map));
}

Node* CodeStubAssembler::IsJSReceiver(Node* object) {
  return IsJSReceiverMap(LoadMap(object));
}

Node* CodeStubAssembler::IsNullOrJSReceiver(Node* object) {
  return Word32Or(IsJSReceiver(object), IsNull(object));
}

Node* CodeStubAssembler::IsNullOrUndefined(Node* const value) {
  return Word32Or(IsUndefined(value), IsNull(value));
}

Node* CodeStubAssembler::IsJSGlobalProxyInstanceType(Node* instance_type) {
  return InstanceTypeEqual(instance_type, JS_GLOBAL_PROXY_TYPE);
}

Node* CodeStubAssembler::IsJSObjectInstanceType(Node* instance_type) {
  STATIC_ASSERT(LAST_JS_OBJECT_TYPE == LAST_TYPE);
  return Int32GreaterThanOrEqual(instance_type,
                                 Int32Constant(FIRST_JS_OBJECT_TYPE));
}

Node* CodeStubAssembler::IsJSObjectMap(Node* map) {
  CSA_ASSERT(this, IsMap(map));
  return IsJSObjectInstanceType(LoadMapInstanceType(map));
}

Node* CodeStubAssembler::IsJSObject(Node* object) {
  return IsJSObjectMap(LoadMap(object));
}

Node* CodeStubAssembler::IsJSProxy(Node* object) {
  return HasInstanceType(object, JS_PROXY_TYPE);
}

Node* CodeStubAssembler::IsJSGlobalProxy(Node* object) {
  return HasInstanceType(object, JS_GLOBAL_PROXY_TYPE);
}

Node* CodeStubAssembler::IsMap(Node* map) { return IsMetaMap(LoadMap(map)); }

Node* CodeStubAssembler::IsJSValueInstanceType(Node* instance_type) {
  return InstanceTypeEqual(instance_type, JS_VALUE_TYPE);
}

Node* CodeStubAssembler::IsJSValue(Node* object) {
  return IsJSValueMap(LoadMap(object));
}

Node* CodeStubAssembler::IsJSValueMap(Node* map) {
  return IsJSValueInstanceType(LoadMapInstanceType(map));
}

Node* CodeStubAssembler::IsJSArrayInstanceType(Node* instance_type) {
  return InstanceTypeEqual(instance_type, JS_ARRAY_TYPE);
}

Node* CodeStubAssembler::IsJSArray(Node* object) {
  return IsJSArrayMap(LoadMap(object));
}

Node* CodeStubAssembler::IsJSArrayMap(Node* map) {
  return IsJSArrayInstanceType(LoadMapInstanceType(map));
}

Node* CodeStubAssembler::IsFixedArray(Node* object) {
  return HasInstanceType(object, FIXED_ARRAY_TYPE);
}

Node* CodeStubAssembler::IsFixedArraySubclass(Node* object) {
  Node* instance_type = LoadInstanceType(object);
  return Word32And(Int32GreaterThanOrEqual(
                       instance_type, Int32Constant(FIRST_FIXED_ARRAY_TYPE)),
                   Int32LessThanOrEqual(instance_type,
                                        Int32Constant(LAST_FIXED_ARRAY_TYPE)));
}

Node* CodeStubAssembler::IsPropertyArray(Node* object) {
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
Node* CodeStubAssembler::IsFixedArrayWithKindOrEmpty(Node* object,
                                                     ElementsKind kind) {
  Label out(this);
  VARIABLE(var_result, MachineRepresentation::kWord32, Int32Constant(1));

  GotoIf(IsFixedArrayWithKind(object, kind), &out);

  Node* const length = LoadFixedArrayBaseLength(object);
  GotoIf(SmiEqual(length, SmiConstant(0)), &out);

  var_result.Bind(Int32Constant(0));
  Goto(&out);

  BIND(&out);
  return var_result.value();
}

Node* CodeStubAssembler::IsFixedArrayWithKind(Node* object, ElementsKind kind) {
  if (IsDoubleElementsKind(kind)) {
    return IsFixedDoubleArray(object);
  } else {
    DCHECK(IsSmiOrObjectElementsKind(kind));
    return IsFixedArraySubclass(object);
  }
}

Node* CodeStubAssembler::IsWeakCell(Node* object) {
  return IsWeakCellMap(LoadMap(object));
}

Node* CodeStubAssembler::IsBoolean(Node* object) {
  return IsBooleanMap(LoadMap(object));
}

Node* CodeStubAssembler::IsPropertyCell(Node* object) {
  return IsPropertyCellMap(LoadMap(object));
}

Node* CodeStubAssembler::IsAccessorInfo(Node* object) {
  return IsAccessorInfoMap(LoadMap(object));
}

Node* CodeStubAssembler::IsAccessorPair(Node* object) {
  return IsAccessorPairMap(LoadMap(object));
}

Node* CodeStubAssembler::IsAllocationSite(Node* object) {
  return IsAllocationSiteMap(LoadMap(object));
}

Node* CodeStubAssembler::IsAnyHeapNumber(Node* object) {
  return Word32Or(IsMutableHeapNumber(object), IsHeapNumber(object));
}

Node* CodeStubAssembler::IsHeapNumber(Node* object) {
  return IsHeapNumberMap(LoadMap(object));
}

Node* CodeStubAssembler::IsMutableHeapNumber(Node* object) {
  return IsMutableHeapNumberMap(LoadMap(object));
}

Node* CodeStubAssembler::IsFeedbackVector(Node* object) {
  return IsFeedbackVectorMap(LoadMap(object));
}

Node* CodeStubAssembler::IsName(Node* object) {
  return Int32LessThanOrEqual(LoadInstanceType(object),
                              Int32Constant(LAST_NAME_TYPE));
}

Node* CodeStubAssembler::IsString(Node* object) {
  return IsStringInstanceType(LoadInstanceType(object));
}

Node* CodeStubAssembler::IsSymbolInstanceType(Node* instance_type) {
  return InstanceTypeEqual(instance_type, SYMBOL_TYPE);
}

Node* CodeStubAssembler::IsSymbol(Node* object) {
  return IsSymbolMap(LoadMap(object));
}

Node* CodeStubAssembler::IsBigIntInstanceType(Node* instance_type) {
  return InstanceTypeEqual(instance_type, BIGINT_TYPE);
}

Node* CodeStubAssembler::IsBigInt(Node* object) {
  return IsBigIntInstanceType(LoadInstanceType(object));
}

Node* CodeStubAssembler::IsPrimitiveInstanceType(Node* instance_type) {
  return Int32LessThanOrEqual(instance_type,
                              Int32Constant(LAST_PRIMITIVE_TYPE));
}

Node* CodeStubAssembler::IsPrivateSymbol(Node* object) {
  return Select(
      IsSymbol(object),
      [=] {
        TNode<Symbol> symbol = CAST(object);
        TNode<Int32T> flags =
            SmiToWord32(LoadObjectField<Smi>(symbol, Symbol::kFlagsOffset));
        return IsSetWord32(flags, 1 << Symbol::kPrivateBit);
      },
      [=] { return Int32Constant(0); }, MachineRepresentation::kWord32);
}

Node* CodeStubAssembler::IsNativeContext(Node* object) {
  return WordEqual(LoadMap(object), LoadRoot(Heap::kNativeContextMapRootIndex));
}

Node* CodeStubAssembler::IsFixedDoubleArray(Node* object) {
  return WordEqual(LoadMap(object), FixedDoubleArrayMapConstant());
}

Node* CodeStubAssembler::IsHashTable(Node* object) {
  return HasInstanceType(object, HASH_TABLE_TYPE);
}

Node* CodeStubAssembler::IsDictionary(Node* object) {
  return Word32Or(IsHashTable(object), IsNumberDictionary(object));
}

Node* CodeStubAssembler::IsNumberDictionary(Node* object) {
  return WordEqual(LoadMap(object),
                   LoadRoot(Heap::kNumberDictionaryMapRootIndex));
}

Node* CodeStubAssembler::IsJSFunctionInstanceType(Node* instance_type) {
  return InstanceTypeEqual(instance_type, JS_FUNCTION_TYPE);
}

Node* CodeStubAssembler::IsJSFunction(Node* object) {
  return IsJSFunctionMap(LoadMap(object));
}

Node* CodeStubAssembler::IsJSFunctionMap(Node* map) {
  return IsJSFunctionInstanceType(LoadMapInstanceType(map));
}

Node* CodeStubAssembler::IsJSTypedArray(Node* object) {
  return HasInstanceType(object, JS_TYPED_ARRAY_TYPE);
}

Node* CodeStubAssembler::IsJSArrayBuffer(Node* object) {
  return HasInstanceType(object, JS_ARRAY_BUFFER_TYPE);
}

Node* CodeStubAssembler::IsFixedTypedArray(Node* object) {
  Node* instance_type = LoadInstanceType(object);
  return Word32And(
      Int32GreaterThanOrEqual(instance_type,
                              Int32Constant(FIRST_FIXED_TYPED_ARRAY_TYPE)),
      Int32LessThanOrEqual(instance_type,
                           Int32Constant(LAST_FIXED_TYPED_ARRAY_TYPE)));
}

Node* CodeStubAssembler::IsJSRegExp(Node* object) {
  return HasInstanceType(object, JS_REGEXP_TYPE);
}

Node* CodeStubAssembler::IsNumeric(Node* object) {
  return Select(
      TaggedIsSmi(object), [=] { return Int32Constant(1); },
      [=] { return Word32Or(IsHeapNumber(object), IsBigInt(object)); },
      MachineRepresentation::kWord32);
}

Node* CodeStubAssembler::IsNumber(Node* object) {
  return Select(TaggedIsSmi(object), [=] { return Int32Constant(1); },
                [=] { return IsHeapNumber(object); },
                MachineRepresentation::kWord32);
}

Node* CodeStubAssembler::FixedArraySizeDoesntFitInNewSpace(Node* element_count,
                                                           int base_size,
                                                           ParameterMode mode) {
  int max_newspace_elements =
      (kMaxRegularHeapObjectSize - base_size) / kPointerSize;
  return IntPtrOrSmiGreaterThan(
      element_count, IntPtrOrSmiConstant(max_newspace_elements, mode), mode);
}

Node* CodeStubAssembler::IsNumberNormalized(Node* number) {
  CSA_ASSERT(this, IsNumber(number));

  VARIABLE(var_result, MachineRepresentation::kWord32, Int32Constant(1));
  Label out(this);

  GotoIf(TaggedIsSmi(number), &out);

  Node* const value = LoadHeapNumberValue(number);
  Node* const smi_min = Float64Constant(static_cast<double>(Smi::kMinValue));
  Node* const smi_max = Float64Constant(static_cast<double>(Smi::kMaxValue));

  GotoIf(Float64LessThan(value, smi_min), &out);
  GotoIf(Float64GreaterThan(value, smi_max), &out);
  GotoIfNot(Float64Equal(value, value), &out);  // NaN.

  var_result.Bind(Int32Constant(0));
  Goto(&out);

  BIND(&out);
  return var_result.value();
}

Node* CodeStubAssembler::IsNumberPositive(Node* number) {
  CSA_ASSERT(this, IsNumber(number));
  Node* const float_zero = Float64Constant(0.);
  return Select(TaggedIsSmi(number),
                [=] { return TaggedIsPositiveSmi(number); },
                [=] {
                  Node* v = LoadHeapNumberValue(number);
                  return Float64GreaterThanOrEqual(v, float_zero);
                },
                MachineRepresentation::kWord32);
}

Node* CodeStubAssembler::IsNumberArrayIndex(Node* number) {
  VARIABLE(var_result, MachineRepresentation::kWord32, Int32Constant(1));

  Label check_upper_bound(this), check_is_integer(this), out(this),
      return_false(this);

  GotoIfNumericGreaterThanOrEqual(number, NumberConstant(0),
                                  &check_upper_bound);
  Goto(&return_false);

  BIND(&check_upper_bound);
  GotoIfNumericGreaterThanOrEqual(number, NumberConstant(kMaxUInt32),
                                  &return_false);
  Goto(&check_is_integer);

  BIND(&check_is_integer);
  GotoIf(TaggedIsSmi(number), &out);
  // Check that the HeapNumber is a valid uint32
  Node* value = LoadHeapNumberValue(number);
  Node* int_value = ChangeFloat64ToUint32(value);
  GotoIf(Float64Equal(value, ChangeUint32ToFloat64(int_value)), &out);
  Goto(&return_false);

  BIND(&return_false);
  var_result.Bind(Int32Constant(0));
  Goto(&out);

  BIND(&out);
  return var_result.value();
}

TNode<Uint32T> CodeStubAssembler::StringCharCodeAt(SloppyTNode<String> string,
                                                   SloppyTNode<IntPtrT> index) {
  CSA_ASSERT(this, IsString(string));

  CSA_ASSERT(this, IntPtrGreaterThanOrEqual(index, IntPtrConstant(0)));
  CSA_ASSERT(this, IntPtrLessThan(index, LoadStringLengthAsWord(string)));

  VARIABLE(var_result, MachineRepresentation::kWord32);

  Label return_result(this), if_runtime(this, Label::kDeferred),
      if_stringistwobyte(this), if_stringisonebyte(this);

  ToDirectStringAssembler to_direct(state(), string);
  to_direct.TryToDirect(&if_runtime);
  Node* const offset = IntPtrAdd(index, to_direct.offset());
  Node* const instance_type = to_direct.instance_type();

  Node* const string_data = to_direct.PointerToData(&if_runtime);

  // Check if the {string} is a TwoByteSeqString or a OneByteSeqString.
  Branch(IsOneByteStringInstanceType(instance_type), &if_stringisonebyte,
         &if_stringistwobyte);

  BIND(&if_stringisonebyte);
  {
    var_result.Bind(Load(MachineType::Uint8(), string_data, offset));
    Goto(&return_result);
  }

  BIND(&if_stringistwobyte);
  {
    var_result.Bind(Load(MachineType::Uint16(), string_data,
                         WordShl(offset, IntPtrConstant(1))));
    Goto(&return_result);
  }

  BIND(&if_runtime);
  {
    Node* result = CallRuntime(Runtime::kStringCharCodeAt, NoContextConstant(),
                               string, SmiTag(index));
    var_result.Bind(SmiToWord32(result));
    Goto(&return_result);
  }

  BIND(&return_result);
  return UncheckedCast<Uint32T>(var_result.value());
}

Node* CodeStubAssembler::StringFromCharCode(Node* code) {
  VARIABLE(var_result, MachineRepresentation::kTagged);

  // Check if the {code} is a one-byte char code.
  Label if_codeisonebyte(this), if_codeistwobyte(this, Label::kDeferred),
      if_done(this);
  Branch(Int32LessThanOrEqual(code, Int32Constant(String::kMaxOneByteCharCode)),
         &if_codeisonebyte, &if_codeistwobyte);
  BIND(&if_codeisonebyte);
  {
    // Load the isolate wide single character string cache.
    Node* cache = LoadRoot(Heap::kSingleCharacterStringCacheRootIndex);
    Node* code_index = ChangeUint32ToWord(code);

    // Check if we have an entry for the {code} in the single character string
    // cache already.
    Label if_entryisundefined(this, Label::kDeferred),
        if_entryisnotundefined(this);
    Node* entry = LoadFixedArrayElement(cache, code_index);
    Branch(IsUndefined(entry), &if_entryisundefined, &if_entryisnotundefined);

    BIND(&if_entryisundefined);
    {
      // Allocate a new SeqOneByteString for {code} and store it in the {cache}.
      Node* result = AllocateSeqOneByteString(1);
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
    Node* result = AllocateSeqTwoByteString(1);
    StoreNoWriteBarrier(
        MachineRepresentation::kWord16, result,
        IntPtrConstant(SeqTwoByteString::kHeaderSize - kHeapObjectTag), code);
    var_result.Bind(result);
    Goto(&if_done);
  }

  BIND(&if_done);
  CSA_ASSERT(this, IsString(var_result.value()));
  return var_result.value();
}

// A wrapper around CopyStringCharacters which determines the correct string
// encoding, allocates a corresponding sequential string, and then copies the
// given character range using CopyStringCharacters.
// |from_string| must be a sequential string.
// 0 <= |from_index| <= |from_index| + |character_count| < from_string.length.
Node* CodeStubAssembler::AllocAndCopyStringCharacters(
    Node* context, Node* from, Node* from_instance_type,
    TNode<IntPtrT> from_index, TNode<Smi> character_count) {
  Label end(this), one_byte_sequential(this), two_byte_sequential(this);
  Variable var_result(this, MachineRepresentation::kTagged);

  Branch(IsOneByteStringInstanceType(from_instance_type), &one_byte_sequential,
         &two_byte_sequential);

  // The subject string is a sequential one-byte string.
  BIND(&one_byte_sequential);
  {
    Node* result = AllocateSeqOneByteString(context, character_count);
    CopyStringCharacters(from, result, from_index, IntPtrConstant(0),
                         SmiUntag(character_count), String::ONE_BYTE_ENCODING,
                         String::ONE_BYTE_ENCODING);
    var_result.Bind(result);

    Goto(&end);
  }

  // The subject string is a sequential two-byte string.
  BIND(&two_byte_sequential);
  {
    Node* result = AllocateSeqTwoByteString(context, character_count);
    CopyStringCharacters(from, result, from_index, IntPtrConstant(0),
                         SmiUntag(character_count), String::TWO_BYTE_ENCODING,
                         String::TWO_BYTE_ENCODING);
    var_result.Bind(result);

    Goto(&end);
  }

  BIND(&end);
  return var_result.value();
}


Node* CodeStubAssembler::SubString(Node* context, Node* string, Node* from,
                                   Node* to, SubStringFlags flags) {
  DCHECK(flags == SubStringFlags::NONE ||
         flags == SubStringFlags::FROM_TO_ARE_BOUNDED);
  VARIABLE(var_result, MachineRepresentation::kTagged);
  ToDirectStringAssembler to_direct(state(), string);
  Label end(this), runtime(this);

  // Make sure first argument is a string.
  CSA_ASSERT(this, TaggedIsNotSmi(string));
  CSA_ASSERT(this, IsString(string));

  // Make sure that both from and to are non-negative smis.

  if (flags == SubStringFlags::NONE) {
    GotoIfNot(TaggedIsPositiveSmi(from), &runtime);
    GotoIfNot(TaggedIsPositiveSmi(to), &runtime);
  } else {
    CSA_ASSERT(this, TaggedIsPositiveSmi(from));
    CSA_ASSERT(this, TaggedIsPositiveSmi(to));
  }

  TNode<Smi> const substr_length = SmiSub(to, from);
  TNode<Smi> const string_length = LoadStringLengthAsSmi(string);

  // Begin dispatching based on substring length.

  Label original_string_or_invalid_length(this);
  GotoIf(SmiAboveOrEqual(substr_length, string_length),
         &original_string_or_invalid_length);

  // A real substring (substr_length < string_length).

  Label single_char(this);
  GotoIf(SmiEqual(substr_length, SmiConstant(1)), &single_char);

  // TODO(jgruber): Add an additional case for substring of length == 0?

  // Deal with different string types: update the index if necessary
  // and extract the underlying string.

  Node* const direct_string = to_direct.TryToDirect(&runtime);
  Node* const offset = SmiAdd(from, SmiTag(to_direct.offset()));
  Node* const instance_type = to_direct.instance_type();

  // The subject string can only be external or sequential string of either
  // encoding at this point.
  Label external_string(this);
  {
    if (FLAG_string_slices) {
      Label next(this);

      // Short slice.  Copy instead of slicing.
      GotoIf(SmiLessThan(substr_length, SmiConstant(SlicedString::kMinLength)),
             &next);

      // Allocate new sliced string.

      Counters* counters = isolate()->counters();
      IncrementCounter(counters->sub_string_native(), 1);

      Label one_byte_slice(this), two_byte_slice(this);
      Branch(IsOneByteStringInstanceType(to_direct.instance_type()),
             &one_byte_slice, &two_byte_slice);

      BIND(&one_byte_slice);
      {
        var_result.Bind(
            AllocateSlicedOneByteString(substr_length, direct_string, offset));
        Goto(&end);
      }

      BIND(&two_byte_slice);
      {
        var_result.Bind(
            AllocateSlicedTwoByteString(substr_length, direct_string, offset));
        Goto(&end);
      }

      BIND(&next);
    }

    // The subject string can only be external or sequential string of either
    // encoding at this point.
    GotoIf(to_direct.is_external(), &external_string);

    var_result.Bind(
        AllocAndCopyStringCharacters(context, direct_string, instance_type,
                                     SmiUntag(offset), substr_length));

    Counters* counters = isolate()->counters();
    IncrementCounter(counters->sub_string_native(), 1);

    Goto(&end);
  }

  // Handle external string.
  BIND(&external_string);
  {
    Node* const fake_sequential_string = to_direct.PointerToString(&runtime);

    var_result.Bind(AllocAndCopyStringCharacters(
        context, fake_sequential_string, instance_type, SmiUntag(offset),
        substr_length));

    Counters* counters = isolate()->counters();
    IncrementCounter(counters->sub_string_native(), 1);

    Goto(&end);
  }

  // Substrings of length 1 are generated through CharCodeAt and FromCharCode.
  BIND(&single_char);
  {
    Node* char_code = StringCharCodeAt(string, SmiUntag(from));
    var_result.Bind(StringFromCharCode(char_code));
    Goto(&end);
  }

  BIND(&original_string_or_invalid_length);
  {
    if (flags == SubStringFlags::NONE) {
      // Longer than original string's length or negative: unsafe arguments.
      GotoIf(SmiAbove(substr_length, string_length), &runtime);
    } else {
      // with flag SubStringFlags::FROM_TO_ARE_BOUNDED, the only way we can
      // get here is if substr_length is equal to string_length.
      CSA_ASSERT(this, SmiEqual(substr_length, string_length));
    }

    // Equal length - check if {from, to} == {0, str.length}.
    GotoIf(SmiAbove(from, SmiConstant(0)), &runtime);

    // Return the original string (substr_length == string_length).

    Counters* counters = isolate()->counters();
    IncrementCounter(counters->sub_string_native(), 1);

    var_result.Bind(string);
    Goto(&end);
  }

  // Fall back to a runtime call.
  BIND(&runtime);
  {
    var_result.Bind(
        CallRuntime(Runtime::kSubString, context, string, from, to));
    Goto(&end);
  }

  BIND(&end);
  CSA_ASSERT(this, IsString(var_result.value()));
  return var_result.value();
}

ToDirectStringAssembler::ToDirectStringAssembler(
    compiler::CodeAssemblerState* state, Node* string, Flags flags)
    : CodeStubAssembler(state),
      var_string_(this, MachineRepresentation::kTagged, string),
      var_instance_type_(this, MachineRepresentation::kWord32),
      var_offset_(this, MachineType::PointerRepresentation()),
      var_is_external_(this, MachineRepresentation::kWord32),
      flags_(flags) {
  CSA_ASSERT(this, TaggedIsNotSmi(string));
  CSA_ASSERT(this, IsString(string));

  var_string_.Bind(string);
  var_offset_.Bind(IntPtrConstant(0));
  var_instance_type_.Bind(LoadInstanceType(string));
  var_is_external_.Bind(Int32Constant(0));
}

Node* ToDirectStringAssembler::TryToDirect(Label* if_bailout) {
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

    Node* const representation = Word32And(
        var_instance_type_.value(), Int32Constant(kStringRepresentationMask));
    Switch(representation, if_bailout, values, labels, arraysize(values));
  }

  // Cons string.  Check whether it is flat, then fetch first part.
  // Flat cons strings have an empty second part.
  BIND(&if_iscons);
  {
    Node* const string = var_string_.value();
    GotoIfNot(IsEmptyString(LoadObjectField(string, ConsString::kSecondOffset)),
              if_bailout);

    Node* const lhs = LoadObjectField(string, ConsString::kFirstOffset);
    var_string_.Bind(lhs);
    var_instance_type_.Bind(LoadInstanceType(lhs));

    Goto(&dispatch);
  }

  // Sliced string. Fetch parent and correct start index by offset.
  BIND(&if_issliced);
  {
    if (!FLAG_string_slices || (flags_ & kDontUnpackSlicedStrings)) {
      Goto(if_bailout);
    } else {
      Node* const string = var_string_.value();
      Node* const sliced_offset =
          LoadAndUntagObjectField(string, SlicedString::kOffsetOffset);
      var_offset_.Bind(IntPtrAdd(var_offset_.value(), sliced_offset));

      Node* const parent = LoadObjectField(string, SlicedString::kParentOffset);
      var_string_.Bind(parent);
      var_instance_type_.Bind(LoadInstanceType(parent));

      Goto(&dispatch);
    }
  }

  // Thin string. Fetch the actual string.
  BIND(&if_isthin);
  {
    Node* const string = var_string_.value();
    Node* const actual_string =
        LoadObjectField(string, ThinString::kActualOffset);
    Node* const actual_instance_type = LoadInstanceType(actual_string);

    var_string_.Bind(actual_string);
    var_instance_type_.Bind(actual_instance_type);

    Goto(&dispatch);
  }

  // External string.
  BIND(&if_isexternal);
  var_is_external_.Bind(Int32Constant(1));
  Goto(&out);

  BIND(&out);
  return var_string_.value();
}

Node* ToDirectStringAssembler::TryToSequential(StringPointerKind ptr_kind,
                                               Label* if_bailout) {
  CHECK(ptr_kind == PTR_TO_DATA || ptr_kind == PTR_TO_STRING);

  VARIABLE(var_result, MachineType::PointerRepresentation());
  Label out(this), if_issequential(this), if_isexternal(this, Label::kDeferred);
  Branch(is_external(), &if_isexternal, &if_issequential);

  BIND(&if_issequential);
  {
    STATIC_ASSERT(SeqOneByteString::kHeaderSize ==
                  SeqTwoByteString::kHeaderSize);
    Node* result = BitcastTaggedToWord(var_string_.value());
    if (ptr_kind == PTR_TO_DATA) {
      result = IntPtrAdd(result, IntPtrConstant(SeqOneByteString::kHeaderSize -
                                                kHeapObjectTag));
    }
    var_result.Bind(result);
    Goto(&out);
  }

  BIND(&if_isexternal);
  {
    GotoIf(IsShortExternalStringInstanceType(var_instance_type_.value()),
           if_bailout);

    Node* const string = var_string_.value();
    Node* result = LoadObjectField(string, ExternalString::kResourceDataOffset,
                                   MachineType::Pointer());
    if (ptr_kind == PTR_TO_STRING) {
      result = IntPtrSub(result, IntPtrConstant(SeqOneByteString::kHeaderSize -
                                                kHeapObjectTag));
    }
    var_result.Bind(result);
    Goto(&out);
  }

  BIND(&out);
  return var_result.value();
}

void CodeStubAssembler::BranchIfCanDerefIndirectString(Node* string,
                                                       Node* instance_type,
                                                       Label* can_deref,
                                                       Label* cannot_deref) {
  CSA_ASSERT(this, IsString(string));
  Node* representation =
      Word32And(instance_type, Int32Constant(kStringRepresentationMask));
  GotoIf(Word32Equal(representation, Int32Constant(kThinStringTag)), can_deref);
  GotoIf(Word32NotEqual(representation, Int32Constant(kConsStringTag)),
         cannot_deref);
  // Cons string.
  Node* rhs = LoadObjectField(string, ConsString::kSecondOffset);
  GotoIf(IsEmptyString(rhs), can_deref);
  Goto(cannot_deref);
}

void CodeStubAssembler::DerefIndirectString(Variable* var_string,
                                            Node* instance_type) {
#ifdef DEBUG
  Label can_deref(this), cannot_deref(this);
  BranchIfCanDerefIndirectString(var_string->value(), instance_type, &can_deref,
                                 &cannot_deref);
  BIND(&cannot_deref);
  DebugBreak();  // Should be able to dereference string.
  Goto(&can_deref);
  BIND(&can_deref);
#endif  // DEBUG

  STATIC_ASSERT(ThinString::kActualOffset == ConsString::kFirstOffset);
  var_string->Bind(
      LoadObjectField(var_string->value(), ThinString::kActualOffset));
}

void CodeStubAssembler::MaybeDerefIndirectString(Variable* var_string,
                                                 Node* instance_type,
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

void CodeStubAssembler::MaybeDerefIndirectStrings(Variable* var_left,
                                                  Node* left_instance_type,
                                                  Variable* var_right,
                                                  Node* right_instance_type,
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

Node* CodeStubAssembler::StringAdd(Node* context, Node* left, Node* right,
                                   AllocationFlags flags) {
  VARIABLE(result, MachineRepresentation::kTagged);
  Label check_right(this), runtime(this, Label::kDeferred), cons(this),
      done(this, &result), done_native(this, &result);
  Counters* counters = isolate()->counters();

  TNode<Smi> left_length = LoadStringLengthAsSmi(left);
  GotoIf(SmiNotEqual(SmiConstant(0), left_length), &check_right);
  result.Bind(right);
  Goto(&done_native);

  BIND(&check_right);
  TNode<Smi> right_length = LoadStringLengthAsSmi(right);
  GotoIf(SmiNotEqual(SmiConstant(0), right_length), &cons);
  result.Bind(left);
  Goto(&done_native);

  BIND(&cons);
  {
    TNode<Smi> new_length = SmiAdd(left_length, right_length);

    // If new length is greater than String::kMaxLength, goto runtime to
    // throw. Note: we also need to invalidate the string length protector, so
    // can't just throw here directly.
    GotoIf(SmiGreaterThan(new_length, SmiConstant(String::kMaxLength)),
           &runtime);

    VARIABLE(var_left, MachineRepresentation::kTagged, left);
    VARIABLE(var_right, MachineRepresentation::kTagged, right);
    Variable* input_vars[2] = {&var_left, &var_right};
    Label non_cons(this, 2, input_vars);
    Label slow(this, Label::kDeferred);
    GotoIf(SmiLessThan(new_length, SmiConstant(ConsString::kMinLength)),
           &non_cons);

    result.Bind(NewConsString(context, new_length, var_left.value(),
                              var_right.value(), flags));
    Goto(&done_native);

    BIND(&non_cons);

    Comment("Full string concatenate");
    Node* left_instance_type = LoadInstanceType(var_left.value());
    Node* right_instance_type = LoadInstanceType(var_right.value());
    // Compute intersection and difference of instance types.

    Node* ored_instance_types =
        Word32Or(left_instance_type, right_instance_type);
    Node* xored_instance_types =
        Word32Xor(left_instance_type, right_instance_type);

    // Check if both strings have the same encoding and both are sequential.
    GotoIf(IsSetWord32(xored_instance_types, kStringEncodingMask), &runtime);
    GotoIf(IsSetWord32(ored_instance_types, kStringRepresentationMask), &slow);

    TNode<IntPtrT> word_left_length = SmiUntag(left_length);
    TNode<IntPtrT> word_right_length = SmiUntag(right_length);

    Label two_byte(this);
    GotoIf(Word32Equal(Word32And(ored_instance_types,
                                 Int32Constant(kStringEncodingMask)),
                       Int32Constant(kTwoByteStringTag)),
           &two_byte);
    // One-byte sequential string case
    Node* new_string = AllocateSeqOneByteString(context, new_length);
    CopyStringCharacters(var_left.value(), new_string, IntPtrConstant(0),
                         IntPtrConstant(0), word_left_length,
                         String::ONE_BYTE_ENCODING, String::ONE_BYTE_ENCODING);
    CopyStringCharacters(var_right.value(), new_string, IntPtrConstant(0),
                         word_left_length, word_right_length,
                         String::ONE_BYTE_ENCODING, String::ONE_BYTE_ENCODING);
    result.Bind(new_string);
    Goto(&done_native);

    BIND(&two_byte);
    {
      // Two-byte sequential string case
      new_string = AllocateSeqTwoByteString(context, new_length);
      CopyStringCharacters(var_left.value(), new_string, IntPtrConstant(0),
                           IntPtrConstant(0), word_left_length,
                           String::TWO_BYTE_ENCODING,
                           String::TWO_BYTE_ENCODING);
      CopyStringCharacters(var_right.value(), new_string, IntPtrConstant(0),
                           word_left_length, word_right_length,
                           String::TWO_BYTE_ENCODING,
                           String::TWO_BYTE_ENCODING);
      result.Bind(new_string);
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
    result.Bind(CallRuntime(Runtime::kStringAdd, context, left, right));
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

Node* CodeStubAssembler::StringFromCodePoint(Node* codepoint,
                                             UnicodeEncoding encoding) {
  VARIABLE(var_result, MachineRepresentation::kTagged, EmptyStringConstant());

  Label if_isword16(this), if_isword32(this), return_result(this);

  Branch(Uint32LessThan(codepoint, Int32Constant(0x10000)), &if_isword16,
         &if_isword32);

  BIND(&if_isword16);
  {
    var_result.Bind(StringFromCharCode(codepoint));
    Goto(&return_result);
  }

  BIND(&if_isword32);
  {
    switch (encoding) {
      case UnicodeEncoding::UTF16:
        break;
      case UnicodeEncoding::UTF32: {
        // Convert UTF32 to UTF16 code units, and store as a 32 bit word.
        Node* lead_offset = Int32Constant(0xD800 - (0x10000 >> 10));

        // lead = (codepoint >> 10) + LEAD_OFFSET
        Node* lead =
            Int32Add(Word32Shr(codepoint, Int32Constant(10)), lead_offset);

        // trail = (codepoint & 0x3FF) + 0xDC00;
        Node* trail = Int32Add(Word32And(codepoint, Int32Constant(0x3FF)),
                               Int32Constant(0xDC00));

        // codpoint = (trail << 16) | lead;
        codepoint = Word32Or(Word32Shl(trail, Int32Constant(16)), lead);
        break;
      }
    }

    Node* value = AllocateSeqTwoByteString(2);
    StoreNoWriteBarrier(
        MachineRepresentation::kWord32, value,
        IntPtrConstant(SeqTwoByteString::kHeaderSize - kHeapObjectTag),
        codepoint);
    var_result.Bind(value);
    Goto(&return_result);
  }

  BIND(&return_result);
  CSA_ASSERT(this, IsString(var_result.value()));
  return var_result.value();
}

TNode<Number> CodeStubAssembler::StringToNumber(SloppyTNode<Context> context,
                                                SloppyTNode<String> input) {
  CSA_SLOW_ASSERT(this, IsString(input));
  Label runtime(this, Label::kDeferred);
  Label end(this);

  TVARIABLE(Number, var_result);

  // Check if string has a cached array index.
  TNode<Uint32T> hash = LoadNameHashField(input);
  GotoIf(IsSetWord32(hash, Name::kDoesNotContainCachedArrayIndexMask),
         &runtime);

  var_result = SmiTag(DecodeWordFromWord32<String::ArrayIndexValueBits>(hash));
  Goto(&end);

  BIND(&runtime);
  {
    var_result = CAST(CallRuntime(Runtime::kStringToNumber, context, input));
    Goto(&end);
  }

  BIND(&end);
  return var_result;
}

Node* CodeStubAssembler::NumberToString(Node* context, Node* argument) {
  VARIABLE(result, MachineRepresentation::kTagged);
  Label runtime(this, Label::kDeferred), smi(this), done(this, &result);

  // Load the number string cache.
  Node* number_string_cache = LoadRoot(Heap::kNumberStringCacheRootIndex);

  // Make the hash mask from the length of the number string cache. It
  // contains two elements (number and string) for each cache entry.
  // TODO(ishell): cleanup mask handling.
  Node* mask =
      BitcastTaggedToWord(LoadFixedArrayBaseLength(number_string_cache));
  Node* one = IntPtrConstant(1);
  mask = IntPtrSub(mask, one);

  GotoIf(TaggedIsSmi(argument), &smi);

  // Argument isn't smi, check to see if it's a heap-number.
  GotoIfNot(IsHeapNumber(argument), &runtime);

  // Make a hash from the two 32-bit values of the double.
  Node* low =
      LoadObjectField(argument, HeapNumber::kValueOffset, MachineType::Int32());
  Node* high = LoadObjectField(argument, HeapNumber::kValueOffset + kIntSize,
                               MachineType::Int32());
  Node* hash = Word32Xor(low, high);
  hash = ChangeInt32ToIntPtr(hash);
  hash = WordShl(hash, one);
  Node* index = WordAnd(hash, WordSar(mask, SmiShiftBitsConstant()));

  // Cache entry's key must be a heap number
  Node* number_key = LoadFixedArrayElement(number_string_cache, index);
  GotoIf(TaggedIsSmi(number_key), &runtime);
  GotoIfNot(IsHeapNumber(number_key), &runtime);

  // Cache entry's key must match the heap number value we're looking for.
  Node* low_compare = LoadObjectField(number_key, HeapNumber::kValueOffset,
                                      MachineType::Int32());
  Node* high_compare = LoadObjectField(
      number_key, HeapNumber::kValueOffset + kIntSize, MachineType::Int32());
  GotoIfNot(Word32Equal(low, low_compare), &runtime);
  GotoIfNot(Word32Equal(high, high_compare), &runtime);

  // Heap number match, return value from cache entry.
  IncrementCounter(isolate()->counters()->number_to_string_native(), 1);
  result.Bind(LoadFixedArrayElement(number_string_cache, index, kPointerSize));
  Goto(&done);

  BIND(&runtime);
  {
    // No cache entry, go to the runtime.
    result.Bind(CallRuntime(Runtime::kNumberToString, context, argument));
  }
  Goto(&done);

  BIND(&smi);
  {
    // Load the smi key, make sure it matches the smi we're looking for.
    Node* smi_index = BitcastWordToTagged(
        WordAnd(WordShl(BitcastTaggedToWord(argument), one), mask));
    Node* smi_key = LoadFixedArrayElement(number_string_cache, smi_index, 0,
                                          SMI_PARAMETERS);
    GotoIf(WordNotEqual(smi_key, argument), &runtime);

    // Smi match, return value from cache entry.
    IncrementCounter(isolate()->counters()->number_to_string_native(), 1);
    result.Bind(LoadFixedArrayElement(number_string_cache, smi_index,
                                      kPointerSize, SMI_PARAMETERS));
    Goto(&done);
  }

  BIND(&done);
  CSA_ASSERT(this, IsString(result.value()));
  return result.value();
}

Node* CodeStubAssembler::ToName(Node* context, Node* value) {
  Label end(this);
  VARIABLE(var_result, MachineRepresentation::kTagged);

  Label is_number(this);
  GotoIf(TaggedIsSmi(value), &is_number);

  Label not_name(this);
  Node* value_instance_type = LoadInstanceType(value);
  STATIC_ASSERT(FIRST_NAME_TYPE == FIRST_TYPE);
  GotoIf(Int32GreaterThan(value_instance_type, Int32Constant(LAST_NAME_TYPE)),
         &not_name);

  var_result.Bind(value);
  Goto(&end);

  BIND(&is_number);
  {
    var_result.Bind(CallBuiltin(Builtins::kNumberToString, context, value));
    Goto(&end);
  }

  BIND(&not_name);
  {
    GotoIf(InstanceTypeEqual(value_instance_type, HEAP_NUMBER_TYPE),
           &is_number);

    Label not_oddball(this);
    GotoIfNot(InstanceTypeEqual(value_instance_type, ODDBALL_TYPE),
              &not_oddball);

    var_result.Bind(LoadObjectField(value, Oddball::kToStringOffset));
    Goto(&end);

    BIND(&not_oddball);
    {
      var_result.Bind(CallRuntime(Runtime::kToName, context, value));
      Goto(&end);
    }
  }

  BIND(&end);
  CSA_ASSERT(this, IsName(var_result.value()));
  return var_result.value();
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
    Node* input_instance_type = LoadInstanceType(input);
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
      var_result.Bind(StringToNumber(context, input));
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
      Node* result = CallStub(callable, context, input);

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
  return var_result;
}

void CodeStubAssembler::TaggedToNumeric(Node* context, Node* value, Label* done,
                                        Variable* var_numeric) {
  TaggedToNumeric<Feedback::kNone>(context, value, done, var_numeric);
}

void CodeStubAssembler::TaggedToNumericWithFeedback(Node* context, Node* value,
                                                    Label* done,
                                                    Variable* var_numeric,
                                                    Variable* var_feedback) {
  TaggedToNumeric<Feedback::kCollect>(context, value, done, var_numeric,
                                      var_feedback);
}

template <CodeStubAssembler::Feedback feedback>
void CodeStubAssembler::TaggedToNumeric(Node* context, Node* value, Label* done,
                                        Variable* var_numeric,
                                        Variable* var_feedback) {
  var_numeric->Bind(value);
  Label if_smi(this), if_heapnumber(this), if_bigint(this), if_oddball(this);
  GotoIf(TaggedIsSmi(value), &if_smi);
  Node* map = LoadMap(value);
  GotoIf(IsHeapNumberMap(map), &if_heapnumber);
  Node* instance_type = LoadMapInstanceType(map);
  GotoIf(IsBigIntInstanceType(instance_type), &if_bigint);

  // {value} is not a Numeric yet.
  GotoIf(Word32Equal(instance_type, Int32Constant(ODDBALL_TYPE)), &if_oddball);
  var_numeric->Bind(CallBuiltin(Builtins::kNonNumberToNumeric, context, value));
  if (feedback == Feedback::kCollect) {
    var_feedback->Bind(SmiConstant(BinaryOperationFeedback::kAny));
  }
  Goto(done);

  BIND(&if_smi);
  if (feedback == Feedback::kCollect) {
    var_feedback->Bind(SmiConstant(BinaryOperationFeedback::kSignedSmall));
  }
  Goto(done);

  BIND(&if_heapnumber);
  if (feedback == Feedback::kCollect) {
    var_feedback->Bind(SmiConstant(BinaryOperationFeedback::kNumber));
  }
  Goto(done);

  BIND(&if_bigint);
  if (feedback == Feedback::kCollect) {
    var_feedback->Bind(SmiConstant(BinaryOperationFeedback::kBigInt));
  }
  Goto(done);

  BIND(&if_oddball);
  var_numeric->Bind(LoadObjectField(value, Oddball::kToNumberOffset));
  if (feedback == Feedback::kCollect) {
    var_feedback->Bind(SmiConstant(BinaryOperationFeedback::kNumberOrOddball));
  }
  Goto(done);
}

// ES#sec-touint32
TNode<Number> CodeStubAssembler::ToUint32(SloppyTNode<Context> context,
                                          SloppyTNode<Object> input) {
  Node* const float_zero = Float64Constant(0.0);
  Node* const float_two_32 = Float64Constant(static_cast<double>(1ULL << 32));

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

  Node* const number = ToNumber(context, input);
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
    Node* const uint32_value = SmiToWord32(number);
    Node* float64_value = ChangeUint32ToFloat64(uint32_value);
    var_result.Bind(AllocateHeapNumberWithValue(float64_value));
    Goto(&out);
  }

  BIND(&if_isheapnumber);
  {
    Label return_zero(this);
    Node* const value = LoadHeapNumberValue(number);

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
      Node* const positive_infinity =
          Float64Constant(std::numeric_limits<double>::infinity());
      Branch(Float64Equal(value, positive_infinity), &return_zero, &next);
      BIND(&next);
    }

    {
      // -Infinity.
      Label next(this);
      Node* const negative_infinity =
          Float64Constant(-1.0 * std::numeric_limits<double>::infinity());
      Branch(Float64Equal(value, negative_infinity), &return_zero, &next);
      BIND(&next);
    }

    // * Let int be the mathematical value that is the same sign as number and
    //   whose magnitude is floor(abs(number)).
    // * Let int32bit be int modulo 2^32.
    // * Return int32bit.
    {
      Node* x = Float64Trunc(value);
      x = Float64Mod(x, float_two_32);
      x = Float64Add(x, float_two_32);
      x = Float64Mod(x, float_two_32);

      Node* const result = ChangeFloat64ToTagged(x);
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

TNode<String> CodeStubAssembler::ToString(SloppyTNode<Context> context,
                                          SloppyTNode<Object> input) {
  Label is_number(this);
  Label runtime(this, Label::kDeferred), done(this);
  VARIABLE(result, MachineRepresentation::kTagged);
  GotoIf(TaggedIsSmi(input), &is_number);

  TNode<Map> input_map = LoadMap(CAST(input));
  TNode<Int32T> input_instance_type = LoadMapInstanceType(input_map);

  result.Bind(input);
  GotoIf(IsStringInstanceType(input_instance_type), &done);

  Label not_heap_number(this);
  Branch(IsHeapNumberMap(input_map), &is_number, &not_heap_number);

  BIND(&is_number);
  result.Bind(NumberToString(context, input));
  Goto(&done);

  BIND(&not_heap_number);
  {
    GotoIfNot(InstanceTypeEqual(input_instance_type, ODDBALL_TYPE), &runtime);
    result.Bind(LoadObjectField(CAST(input), Oddball::kToStringOffset));
    Goto(&done);
  }

  BIND(&runtime);
  {
    result.Bind(CallRuntime(Runtime::kToString, context, input));
    Goto(&done);
  }

  BIND(&done);
  return CAST(result.value());
}

TNode<String> CodeStubAssembler::ToString_Inline(SloppyTNode<Context> context,
                                                 SloppyTNode<Object> input) {
  VARIABLE(var_result, MachineRepresentation::kTagged, input);
  Label stub_call(this, Label::kDeferred), out(this);

  GotoIf(TaggedIsSmi(input), &stub_call);
  Branch(IsString(input), &out, &stub_call);

  BIND(&stub_call);
  var_result.Bind(CallBuiltin(Builtins::kToString, context, input));
  Goto(&out);

  BIND(&out);
  return CAST(var_result.value());
}

Node* CodeStubAssembler::JSReceiverToPrimitive(Node* context, Node* input) {
  Label if_isreceiver(this, Label::kDeferred), if_isnotreceiver(this);
  VARIABLE(result, MachineRepresentation::kTagged);
  Label done(this, &result);

  BranchIfJSReceiver(input, &if_isreceiver, &if_isnotreceiver);

  BIND(&if_isreceiver);
  {
    // Convert {input} to a primitive first passing Number hint.
    Callable callable = CodeFactory::NonPrimitiveToPrimitive(isolate());
    result.Bind(CallStub(callable, context, input));
    Goto(&done);
  }

  BIND(&if_isnotreceiver);
  {
    result.Bind(input);
    Goto(&done);
  }

  BIND(&done);
  return result.value();
}

Node* CodeStubAssembler::ToSmiIndex(Node* const input, Node* const context,
                                    Label* range_error) {
  VARIABLE(result, MachineRepresentation::kTagged, input);
  Label check_undefined(this), return_zero(this), defined(this),
      negative_check(this), done(this);
  Branch(TaggedIsSmi(result.value()), &negative_check, &check_undefined);

  BIND(&check_undefined);
  Branch(IsUndefined(result.value()), &return_zero, &defined);

  BIND(&defined);
  result.Bind(ToInteger(context, result.value(),
                        CodeStubAssembler::kTruncateMinusZero));
  GotoIfNot(TaggedIsSmi(result.value()), range_error);
  CSA_ASSERT(this, TaggedIsSmi(result.value()));
  Goto(&negative_check);

  BIND(&negative_check);
  Branch(SmiLessThan(result.value(), SmiConstant(0)), range_error, &done);

  BIND(&return_zero);
  result.Bind(SmiConstant(0));
  Goto(&done);

  BIND(&done);
  CSA_SLOW_ASSERT(this, TaggedIsSmi(result.value()));
  return result.value();
}

Node* CodeStubAssembler::ToSmiLength(Node* input, Node* const context,
                                     Label* range_error) {
  VARIABLE(result, MachineRepresentation::kTagged, input);
  Label to_integer(this), negative_check(this), return_zero(this), done(this);
  Branch(TaggedIsSmi(result.value()), &negative_check, &to_integer);

  BIND(&to_integer);
  result.Bind(ToInteger(context, result.value(),
                        CodeStubAssembler::kTruncateMinusZero));
  GotoIf(TaggedIsSmi(result.value()), &negative_check);
  // result.value() can still be a negative HeapNumber here.
  Branch(IsTrue(CallBuiltin(Builtins::kLessThan, context, result.value(),
                            SmiConstant(0))),
         &return_zero, range_error);

  BIND(&negative_check);
  Branch(SmiLessThan(result.value(), SmiConstant(0)), &return_zero, &done);

  BIND(&return_zero);
  result.Bind(SmiConstant(0));
  Goto(&done);

  BIND(&done);
  CSA_SLOW_ASSERT(this, TaggedIsSmi(result.value()));
  return result.value();
}

Node* CodeStubAssembler::ToLength_Inline(Node* const context,
                                         Node* const input) {
  Node* const smi_zero = SmiConstant(0);
  return Select(
      TaggedIsSmi(input), [=] { return SmiMax(input, smi_zero); },
      [=] { return CallBuiltin(Builtins::kToLength, context, input); },
      MachineRepresentation::kTagged);
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
    TNode<Object> arg = var_arg;

    // Check if {arg} is a Smi.
    GotoIf(TaggedIsSmi(arg), &out);

    // Check if {arg} is a HeapNumber.
    Label if_argisheapnumber(this),
        if_argisnotheapnumber(this, Label::kDeferred);
    Branch(IsHeapNumber(arg), &if_argisheapnumber, &if_argisnotheapnumber);

    BIND(&if_argisheapnumber);
    {
      TNode<HeapNumber> arg_hn = CAST(arg);
      // Load the floating-point value of {arg}.
      Node* arg_value = LoadHeapNumberValue(arg_hn);

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
  return CAST(var_arg);
}

TNode<Uint32T> CodeStubAssembler::DecodeWord32(SloppyTNode<Word32T> word32,
                                               uint32_t shift, uint32_t mask) {
  return UncheckedCast<Uint32T>(Word32Shr(
      Word32And(word32, Int32Constant(mask)), static_cast<int>(shift)));
}

Node* CodeStubAssembler::DecodeWord(Node* word, uint32_t shift, uint32_t mask) {
  return WordShr(WordAnd(word, IntPtrConstant(mask)), static_cast<int>(shift));
}

Node* CodeStubAssembler::UpdateWord(Node* word, Node* value, uint32_t shift,
                                    uint32_t mask) {
  Node* encoded_value = WordShl(value, static_cast<int>(shift));
  Node* inverted_mask = IntPtrConstant(~static_cast<intptr_t>(mask));
  // Ensure the {value} fits fully in the mask.
  CSA_ASSERT(this, WordEqual(WordAnd(encoded_value, inverted_mask),
                             IntPtrConstant(0)));
  return WordOr(WordAnd(word, inverted_mask), encoded_value);
}

void CodeStubAssembler::SetCounter(StatsCounter* counter, int value) {
  if (FLAG_native_code_counters && counter->Enabled()) {
    Node* counter_address = ExternalConstant(ExternalReference(counter));
    StoreNoWriteBarrier(MachineRepresentation::kWord32, counter_address,
                        Int32Constant(value));
  }
}

void CodeStubAssembler::IncrementCounter(StatsCounter* counter, int delta) {
  DCHECK_GT(delta, 0);
  if (FLAG_native_code_counters && counter->Enabled()) {
    Node* counter_address = ExternalConstant(ExternalReference(counter));
    Node* value = Load(MachineType::Int32(), counter_address);
    value = Int32Add(value, Int32Constant(delta));
    StoreNoWriteBarrier(MachineRepresentation::kWord32, counter_address, value);
  }
}

void CodeStubAssembler::DecrementCounter(StatsCounter* counter, int delta) {
  DCHECK_GT(delta, 0);
  if (FLAG_native_code_counters && counter->Enabled()) {
    Node* counter_address = ExternalConstant(ExternalReference(counter));
    Node* value = Load(MachineType::Int32(), counter_address);
    value = Int32Sub(value, Int32Constant(delta));
    StoreNoWriteBarrier(MachineRepresentation::kWord32, counter_address, value);
  }
}

void CodeStubAssembler::Increment(Variable* variable, int value,
                                  ParameterMode mode) {
  DCHECK_IMPLIES(mode == INTPTR_PARAMETERS,
                 variable->rep() == MachineType::PointerRepresentation());
  DCHECK_IMPLIES(mode == SMI_PARAMETERS,
                 variable->rep() == MachineRepresentation::kTagged ||
                     variable->rep() == MachineRepresentation::kTaggedSigned);
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
  Node* key_map = LoadMap(key);
  var_unique->Bind(key);
  // Symbols are unique.
  GotoIf(IsSymbolMap(key_map), if_keyisunique);
  Node* key_instance_type = LoadMapInstanceType(key_map);
  // Miss if |key| is not a String.
  STATIC_ASSERT(FIRST_NAME_TYPE == FIRST_TYPE);
  GotoIfNot(IsStringInstanceType(key_instance_type), &if_keyisother);

  // |key| is a String. Check if it has a cached array index.
  Node* hash = LoadNameHashField(key);
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
  var_unique->Bind(LoadObjectField(key, ThinString::kActualOffset));
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
  Node* function = ExternalConstant(
      ExternalReference::try_internalize_string_function(isolate()));
  Node* result = CallCFunction1(MachineType::AnyTagged(),
                                MachineType::AnyTagged(), function, string);
  Label internalized(this);
  GotoIf(TaggedIsNotSmi(result), &internalized);
  Node* word_result = SmiUntag(result);
  GotoIf(WordEqual(word_result, IntPtrConstant(ResultSentinel::kNotFound)),
         if_not_internalized);
  GotoIf(WordEqual(word_result, IntPtrConstant(ResultSentinel::kUnsupported)),
         if_bailout);
  var_index->Bind(word_result);
  Goto(if_index);

  BIND(&internalized);
  var_internalized->Bind(result);
  Goto(if_internalized);
}

template <typename Dictionary>
Node* CodeStubAssembler::EntryToIndex(Node* entry, int field_index) {
  Node* entry_index = IntPtrMul(entry, IntPtrConstant(Dictionary::kEntrySize));
  return IntPtrAdd(entry_index, IntPtrConstant(Dictionary::kElementsStartIndex +
                                               field_index));
}

template Node* CodeStubAssembler::EntryToIndex<NameDictionary>(Node*, int);
template Node* CodeStubAssembler::EntryToIndex<GlobalDictionary>(Node*, int);
template Node* CodeStubAssembler::EntryToIndex<NumberDictionary>(Node*, int);

// This must be kept in sync with HashTableBase::ComputeCapacity().
TNode<IntPtrT> CodeStubAssembler::HashTableComputeCapacity(
    SloppyTNode<IntPtrT> at_least_space_for) {
  Node* capacity = IntPtrRoundUpToPowerOfTwo32(
      IntPtrAdd(at_least_space_for, WordShr(at_least_space_for, 1)));
  return IntPtrMax(capacity, IntPtrConstant(HashTableBase::kMinCapacity));
}

TNode<IntPtrT> CodeStubAssembler::IntPtrMax(SloppyTNode<IntPtrT> left,
                                            SloppyTNode<IntPtrT> right) {
  intptr_t left_constant;
  intptr_t right_constant;
  if (ToIntPtrConstant(left, left_constant) &&
      ToIntPtrConstant(right, right_constant)) {
    return IntPtrConstant(std::max(left_constant, right_constant));
  }
  return SelectConstant(IntPtrGreaterThanOrEqual(left, right), left, right,
                        MachineType::PointerRepresentation());
}

TNode<IntPtrT> CodeStubAssembler::IntPtrMin(SloppyTNode<IntPtrT> left,
                                            SloppyTNode<IntPtrT> right) {
  intptr_t left_constant;
  intptr_t right_constant;
  if (ToIntPtrConstant(left, left_constant) &&
      ToIntPtrConstant(right, right_constant)) {
    return IntPtrConstant(std::min(left_constant, right_constant));
  }
  return SelectConstant(IntPtrLessThanOrEqual(left, right), left, right,
                        MachineType::PointerRepresentation());
}

template <class Dictionary>
Node* CodeStubAssembler::GetNextEnumerationIndex(Node* dictionary) {
  return LoadFixedArrayElement(dictionary,
                               Dictionary::kNextEnumerationIndexIndex);
}

template <class Dictionary>
void CodeStubAssembler::SetNextEnumerationIndex(Node* dictionary,
                                                Node* next_enum_index_smi) {
  StoreFixedArrayElement(dictionary, Dictionary::kNextEnumerationIndexIndex,
                         next_enum_index_smi, SKIP_WRITE_BARRIER);
}

template <>
Node* CodeStubAssembler::LoadName<NameDictionary>(Node* key) {
  CSA_ASSERT(this, Word32Or(IsTheHole(key), IsName(key)));
  return key;
}

template <>
Node* CodeStubAssembler::LoadName<GlobalDictionary>(Node* key) {
  CSA_ASSERT(this, IsPropertyCell(key));
  CSA_ASSERT(this, IsNotTheHole(key));
  return LoadObjectField(key, PropertyCell::kNameOffset);
}

template <typename Dictionary>
void CodeStubAssembler::NameDictionaryLookup(Node* dictionary,
                                             Node* unique_name, Label* if_found,
                                             Variable* var_name_index,
                                             Label* if_not_found,
                                             int inlined_probes,
                                             LookupMode mode) {
  CSA_ASSERT(this, IsDictionary(dictionary));
  DCHECK_EQ(MachineType::PointerRepresentation(), var_name_index->rep());
  DCHECK_IMPLIES(mode == kFindInsertionIndex,
                 inlined_probes == 0 && if_found == nullptr);
  Comment("NameDictionaryLookup");

  Node* capacity = SmiUntag(GetCapacity<Dictionary>(dictionary));
  Node* mask = IntPtrSub(capacity, IntPtrConstant(1));
  Node* hash = ChangeUint32ToWord(LoadNameHash(unique_name));

  // See Dictionary::FirstProbe().
  Node* count = IntPtrConstant(0);
  Node* entry = WordAnd(hash, mask);
  Node* undefined = UndefinedConstant();

  for (int i = 0; i < inlined_probes; i++) {
    Node* index = EntryToIndex<Dictionary>(entry);
    var_name_index->Bind(index);

    Node* current = LoadFixedArrayElement(dictionary, index);
    GotoIf(WordEqual(current, undefined), if_not_found);
    current = LoadName<Dictionary>(current);
    GotoIf(WordEqual(current, unique_name), if_found);

    // See Dictionary::NextProbe().
    count = IntPtrConstant(i + 1);
    entry = WordAnd(IntPtrAdd(entry, count), mask);
  }
  if (mode == kFindInsertionIndex) {
    // Appease the variable merging algorithm for "Goto(&loop)" below.
    var_name_index->Bind(IntPtrConstant(0));
  }

  VARIABLE(var_count, MachineType::PointerRepresentation(), count);
  VARIABLE(var_entry, MachineType::PointerRepresentation(), entry);
  Variable* loop_vars[] = {&var_count, &var_entry, var_name_index};
  Label loop(this, 3, loop_vars);
  Goto(&loop);
  BIND(&loop);
  {
    Node* entry = var_entry.value();

    Node* index = EntryToIndex<Dictionary>(entry);
    var_name_index->Bind(index);

    Node* current = LoadFixedArrayElement(dictionary, index);
    GotoIf(WordEqual(current, undefined), if_not_found);
    if (mode == kFindExisting) {
      current = LoadName<Dictionary>(current);
      GotoIf(WordEqual(current, unique_name), if_found);
    } else {
      DCHECK_EQ(kFindInsertionIndex, mode);
      GotoIf(WordEqual(current, TheHoleConstant()), if_not_found);
    }

    // See Dictionary::NextProbe().
    Increment(&var_count);
    entry = WordAnd(IntPtrAdd(entry, var_count.value()), mask);

    var_entry.Bind(entry);
    Goto(&loop);
  }
}

// Instantiate template methods to workaround GCC compilation issue.
template void CodeStubAssembler::NameDictionaryLookup<NameDictionary>(
    Node*, Node*, Label*, Variable*, Label*, int, LookupMode);
template void CodeStubAssembler::NameDictionaryLookup<GlobalDictionary>(
    Node*, Node*, Label*, Variable*, Label*, int, LookupMode);

Node* CodeStubAssembler::ComputeIntegerHash(Node* key) {
  return ComputeIntegerHash(key, IntPtrConstant(kZeroHashSeed));
}

Node* CodeStubAssembler::ComputeIntegerHash(Node* key, Node* seed) {
  // See v8::internal::ComputeIntegerHash()
  Node* hash = TruncateWordToWord32(key);
  hash = Word32Xor(hash, seed);
  hash = Int32Add(Word32Xor(hash, Int32Constant(0xffffffff)),
                  Word32Shl(hash, Int32Constant(15)));
  hash = Word32Xor(hash, Word32Shr(hash, Int32Constant(12)));
  hash = Int32Add(hash, Word32Shl(hash, Int32Constant(2)));
  hash = Word32Xor(hash, Word32Shr(hash, Int32Constant(4)));
  hash = Int32Mul(hash, Int32Constant(2057));
  hash = Word32Xor(hash, Word32Shr(hash, Int32Constant(16)));
  return Word32And(hash, Int32Constant(0x3fffffff));
}

void CodeStubAssembler::NumberDictionaryLookup(Node* dictionary,
                                               Node* intptr_index,
                                               Label* if_found,
                                               Variable* var_entry,
                                               Label* if_not_found) {
  CSA_ASSERT(this, IsNumberDictionary(dictionary));
  DCHECK_EQ(MachineType::PointerRepresentation(), var_entry->rep());
  Comment("NumberDictionaryLookup");

  Node* capacity = SmiUntag(GetCapacity<NumberDictionary>(dictionary));
  Node* mask = IntPtrSub(capacity, IntPtrConstant(1));

  Node* int32_seed = HashSeed();
  Node* hash = ChangeUint32ToWord(ComputeIntegerHash(intptr_index, int32_seed));
  Node* key_as_float64 = RoundIntPtrToFloat64(intptr_index);

  // See Dictionary::FirstProbe().
  Node* count = IntPtrConstant(0);
  Node* entry = WordAnd(hash, mask);

  Node* undefined = UndefinedConstant();
  Node* the_hole = TheHoleConstant();

  VARIABLE(var_count, MachineType::PointerRepresentation(), count);
  Variable* loop_vars[] = {&var_count, var_entry};
  Label loop(this, 2, loop_vars);
  var_entry->Bind(entry);
  Goto(&loop);
  BIND(&loop);
  {
    Node* entry = var_entry->value();

    Node* index = EntryToIndex<NumberDictionary>(entry);
    Node* current = LoadFixedArrayElement(dictionary, index);
    GotoIf(WordEqual(current, undefined), if_not_found);
    Label next_probe(this);
    {
      Label if_currentissmi(this), if_currentisnotsmi(this);
      Branch(TaggedIsSmi(current), &if_currentissmi, &if_currentisnotsmi);
      BIND(&if_currentissmi);
      {
        Node* current_value = SmiUntag(current);
        Branch(WordEqual(current_value, intptr_index), if_found, &next_probe);
      }
      BIND(&if_currentisnotsmi);
      {
        GotoIf(WordEqual(current, the_hole), &next_probe);
        // Current must be the Number.
        Node* current_value = LoadHeapNumberValue(current);
        Branch(Float64Equal(current_value, key_as_float64), if_found,
               &next_probe);
      }
    }

    BIND(&next_probe);
    // See Dictionary::NextProbe().
    Increment(&var_count);
    entry = WordAnd(IntPtrAdd(entry, var_count.value()), mask);

    var_entry->Bind(entry);
    Goto(&loop);
  }
}

template <class Dictionary>
void CodeStubAssembler::FindInsertionEntry(Node* dictionary, Node* key,
                                           Variable* var_key_index) {
  UNREACHABLE();
}

template <>
void CodeStubAssembler::FindInsertionEntry<NameDictionary>(
    Node* dictionary, Node* key, Variable* var_key_index) {
  Label done(this);
  NameDictionaryLookup<NameDictionary>(dictionary, key, nullptr, var_key_index,
                                       &done, 0, kFindInsertionIndex);
  BIND(&done);
}

template <class Dictionary>
void CodeStubAssembler::InsertEntry(Node* dictionary, Node* key, Node* value,
                                    Node* index, Node* enum_index) {
  UNREACHABLE();  // Use specializations instead.
}

template <>
void CodeStubAssembler::InsertEntry<NameDictionary>(Node* dictionary,
                                                    Node* name, Node* value,
                                                    Node* index,
                                                    Node* enum_index) {
  CSA_SLOW_ASSERT(this, IsDictionary(dictionary));

  // Store name and value.
  StoreFixedArrayElement(dictionary, index, name);
  StoreValueByKeyIndex<NameDictionary>(dictionary, index, value);

  // Prepare details of the new property.
  PropertyDetails d(kData, NONE, PropertyCellType::kNoCell);
  enum_index =
      SmiShl(enum_index, PropertyDetails::DictionaryStorageField::kShift);
  // We OR over the actual index below, so we expect the initial value to be 0.
  DCHECK_EQ(0, d.dictionary_index());
  VARIABLE(var_details, MachineRepresentation::kTaggedSigned,
           SmiOr(SmiConstant(d.AsSmi()), enum_index));

  // Private names must be marked non-enumerable.
  Label not_private(this, &var_details);
  GotoIfNot(IsPrivateSymbol(name), &not_private);
  Node* dont_enum =
      SmiShl(SmiConstant(DONT_ENUM), PropertyDetails::AttributesField::kShift);
  var_details.Bind(SmiOr(var_details.value(), dont_enum));
  Goto(&not_private);
  BIND(&not_private);

  // Finally, store the details.
  StoreDetailsByKeyIndex<NameDictionary>(dictionary, index,
                                         var_details.value());
}

template <>
void CodeStubAssembler::InsertEntry<GlobalDictionary>(Node* dictionary,
                                                      Node* key, Node* value,
                                                      Node* index,
                                                      Node* enum_index) {
  UNIMPLEMENTED();
}

template <class Dictionary>
void CodeStubAssembler::Add(Node* dictionary, Node* key, Node* value,
                            Label* bailout) {
  CSA_SLOW_ASSERT(this, IsDictionary(dictionary));
  Node* capacity = GetCapacity<Dictionary>(dictionary);
  Node* nof = GetNumberOfElements<Dictionary>(dictionary);
  Node* new_nof = SmiAdd(nof, SmiConstant(1));
  // Require 33% to still be free after adding additional_elements.
  // Computing "x + (x >> 1)" on a Smi x does not return a valid Smi!
  // But that's OK here because it's only used for a comparison.
  Node* required_capacity_pseudo_smi = SmiAdd(new_nof, SmiShr(new_nof, 1));
  GotoIf(SmiBelow(capacity, required_capacity_pseudo_smi), bailout);
  // Require rehashing if more than 50% of free elements are deleted elements.
  Node* deleted = GetNumberOfDeletedElements<Dictionary>(dictionary);
  CSA_ASSERT(this, SmiAbove(capacity, new_nof));
  Node* half_of_free_elements = SmiShr(SmiSub(capacity, new_nof), 1);
  GotoIf(SmiAbove(deleted, half_of_free_elements), bailout);

  Node* enum_index = GetNextEnumerationIndex<Dictionary>(dictionary);
  Node* new_enum_index = SmiAdd(enum_index, SmiConstant(1));
  Node* max_enum_index =
      SmiConstant(PropertyDetails::DictionaryStorageField::kMax);
  GotoIf(SmiAbove(new_enum_index, max_enum_index), bailout);

  // No more bailouts after this point.
  // Operations from here on can have side effects.

  SetNextEnumerationIndex<Dictionary>(dictionary, new_enum_index);
  SetNumberOfElements<Dictionary>(dictionary, new_nof);

  VARIABLE(var_key_index, MachineType::PointerRepresentation());
  FindInsertionEntry<Dictionary>(dictionary, key, &var_key_index);
  InsertEntry<Dictionary>(dictionary, key, value, var_key_index.value(),
                          enum_index);
}

template void CodeStubAssembler::Add<NameDictionary>(Node*, Node*, Node*,
                                                     Label*);

void CodeStubAssembler::DescriptorLookupLinear(Node* unique_name,
                                               Node* descriptors, Node* nof,
                                               Label* if_found,
                                               Variable* var_name_index,
                                               Label* if_not_found) {
  Comment("DescriptorLookupLinear");
  Node* first_inclusive = IntPtrConstant(DescriptorArray::ToKeyIndex(0));
  Node* factor = IntPtrConstant(DescriptorArray::kEntrySize);
  Node* last_exclusive = IntPtrAdd(first_inclusive, IntPtrMul(nof, factor));

  BuildFastLoop(last_exclusive, first_inclusive,
                [this, descriptors, unique_name, if_found,
                 var_name_index](Node* name_index) {
                  Node* candidate_name =
                      LoadFixedArrayElement(descriptors, name_index);
                  var_name_index->Bind(name_index);
                  GotoIf(WordEqual(candidate_name, unique_name), if_found);
                },
                -DescriptorArray::kEntrySize, INTPTR_PARAMETERS,
                IndexAdvanceMode::kPre);
  Goto(if_not_found);
}

Node* CodeStubAssembler::DescriptorArrayNumberOfEntries(Node* descriptors) {
  return LoadAndUntagToWord32FixedArrayElement(
      descriptors, IntPtrConstant(DescriptorArray::kDescriptorLengthIndex));
}

namespace {

Node* DescriptorNumberToIndex(CodeStubAssembler* a, Node* descriptor_number) {
  Node* descriptor_size = a->Int32Constant(DescriptorArray::kEntrySize);
  Node* index = a->Int32Mul(descriptor_number, descriptor_size);
  return a->ChangeInt32ToIntPtr(index);
}

}  // namespace

Node* CodeStubAssembler::DescriptorArrayToKeyIndex(Node* descriptor_number) {
  return IntPtrAdd(IntPtrConstant(DescriptorArray::ToKeyIndex(0)),
                   DescriptorNumberToIndex(this, descriptor_number));
}

Node* CodeStubAssembler::DescriptorArrayGetSortedKeyIndex(
    Node* descriptors, Node* descriptor_number) {
  const int details_offset = DescriptorArray::ToDetailsIndex(0) * kPointerSize;
  Node* details = LoadAndUntagToWord32FixedArrayElement(
      descriptors, DescriptorNumberToIndex(this, descriptor_number),
      details_offset);
  return DecodeWord32<PropertyDetails::DescriptorPointer>(details);
}

Node* CodeStubAssembler::DescriptorArrayGetKey(Node* descriptors,
                                               Node* descriptor_number) {
  const int key_offset = DescriptorArray::ToKeyIndex(0) * kPointerSize;
  return LoadFixedArrayElement(descriptors,
                               DescriptorNumberToIndex(this, descriptor_number),
                               key_offset);
}

void CodeStubAssembler::DescriptorLookupBinary(Node* unique_name,
                                               Node* descriptors, Node* nof,
                                               Label* if_found,
                                               Variable* var_name_index,
                                               Label* if_not_found) {
  Comment("DescriptorLookupBinary");
  VARIABLE(var_low, MachineRepresentation::kWord32, Int32Constant(0));
  Node* limit =
      Int32Sub(DescriptorArrayNumberOfEntries(descriptors), Int32Constant(1));
  VARIABLE(var_high, MachineRepresentation::kWord32, limit);
  Node* hash = LoadNameHashField(unique_name);
  CSA_ASSERT(this, Word32NotEqual(hash, Int32Constant(0)));

  // Assume non-empty array.
  CSA_ASSERT(this, Uint32LessThanOrEqual(var_low.value(), var_high.value()));

  Variable* loop_vars[] = {&var_high, &var_low};
  Label binary_loop(this, 2, loop_vars);
  Goto(&binary_loop);
  BIND(&binary_loop);
  {
    // mid = low + (high - low) / 2 (to avoid overflow in "(low + high) / 2").
    Node* mid =
        Int32Add(var_low.value(),
                 Word32Shr(Int32Sub(var_high.value(), var_low.value()), 1));
    // mid_name = descriptors->GetSortedKey(mid).
    Node* sorted_key_index = DescriptorArrayGetSortedKeyIndex(descriptors, mid);
    Node* mid_name = DescriptorArrayGetKey(descriptors, sorted_key_index);

    Node* mid_hash = LoadNameHashField(mid_name);

    Label mid_greater(this), mid_less(this), merge(this);
    Branch(Uint32GreaterThanOrEqual(mid_hash, hash), &mid_greater, &mid_less);
    BIND(&mid_greater);
    {
      var_high.Bind(mid);
      Goto(&merge);
    }
    BIND(&mid_less);
    {
      var_low.Bind(Int32Add(mid, Int32Constant(1)));
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

    Node* sort_index =
        DescriptorArrayGetSortedKeyIndex(descriptors, var_low.value());
    Node* current_name = DescriptorArrayGetKey(descriptors, sort_index);
    Node* current_hash = LoadNameHashField(current_name);
    GotoIf(Word32NotEqual(current_hash, hash), if_not_found);
    Label next(this);
    GotoIf(WordNotEqual(current_name, unique_name), &next);
    GotoIf(Int32GreaterThanOrEqual(sort_index, nof), if_not_found);
    var_name_index->Bind(DescriptorArrayToKeyIndex(sort_index));
    Goto(if_found);

    BIND(&next);
    var_low.Bind(Int32Add(var_low.value(), Int32Constant(1)));
    Goto(&scan_loop);
  }
}

void CodeStubAssembler::DescriptorLookup(Node* unique_name, Node* descriptors,
                                         Node* bitfield3, Label* if_found,
                                         Variable* var_name_index,
                                         Label* if_not_found) {
  Comment("DescriptorArrayLookup");
  Node* nof = DecodeWord32<Map::NumberOfOwnDescriptorsBits>(bitfield3);
  GotoIf(Word32Equal(nof, Int32Constant(0)), if_not_found);
  Label linear_search(this), binary_search(this);
  const int kMaxElementsForLinearSearch = 32;
  Branch(Int32LessThanOrEqual(nof, Int32Constant(kMaxElementsForLinearSearch)),
         &linear_search, &binary_search);
  BIND(&linear_search);
  {
    DescriptorLookupLinear(unique_name, descriptors, ChangeInt32ToIntPtr(nof),
                           if_found, var_name_index, if_not_found);
  }
  BIND(&binary_search);
  {
    DescriptorLookupBinary(unique_name, descriptors, nof, if_found,
                           var_name_index, if_not_found);
  }
}

void CodeStubAssembler::TryLookupProperty(
    Node* object, Node* map, Node* instance_type, Node* unique_name,
    Label* if_found_fast, Label* if_found_dict, Label* if_found_global,
    Variable* var_meta_storage, Variable* var_name_index, Label* if_not_found,
    Label* if_bailout) {
  DCHECK_EQ(MachineRepresentation::kTagged, var_meta_storage->rep());
  DCHECK_EQ(MachineType::PointerRepresentation(), var_name_index->rep());

  Label if_objectisspecial(this);
  STATIC_ASSERT(JS_GLOBAL_OBJECT_TYPE <= LAST_SPECIAL_RECEIVER_TYPE);
  GotoIf(Int32LessThanOrEqual(instance_type,
                              Int32Constant(LAST_SPECIAL_RECEIVER_TYPE)),
         &if_objectisspecial);

  uint32_t mask =
      1 << Map::kHasNamedInterceptor | 1 << Map::kIsAccessCheckNeeded;
  CSA_ASSERT(this, Word32BinaryNot(IsSetWord32(LoadMapBitField(map), mask)));
  USE(mask);

  Node* bit_field3 = LoadMapBitField3(map);
  Label if_isfastmap(this), if_isslowmap(this);
  Branch(IsSetWord32<Map::DictionaryMap>(bit_field3), &if_isslowmap,
         &if_isfastmap);
  BIND(&if_isfastmap);
  {
    Node* descriptors = LoadMapDescriptors(map);
    var_meta_storage->Bind(descriptors);

    DescriptorLookup(unique_name, descriptors, bit_field3, if_found_fast,
                     var_name_index, if_not_found);
  }
  BIND(&if_isslowmap);
  {
    Node* dictionary = LoadSlowProperties(object);
    var_meta_storage->Bind(dictionary);

    NameDictionaryLookup<NameDictionary>(dictionary, unique_name, if_found_dict,
                                         var_name_index, if_not_found);
  }
  BIND(&if_objectisspecial);
  {
    // Handle global object here and bailout for other special objects.
    GotoIfNot(InstanceTypeEqual(instance_type, JS_GLOBAL_OBJECT_TYPE),
              if_bailout);

    // Handle interceptors and access checks in runtime.
    Node* bit_field = LoadMapBitField(map);
    int mask = 1 << Map::kHasNamedInterceptor | 1 << Map::kIsAccessCheckNeeded;
    GotoIf(IsSetWord32(bit_field, mask), if_bailout);

    Node* dictionary = LoadSlowProperties(object);
    var_meta_storage->Bind(dictionary);

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
  VARIABLE(var_meta_storage, MachineRepresentation::kTagged);
  VARIABLE(var_name_index, MachineType::PointerRepresentation());

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
  Node* method = GetProperty(context, object, name);

  GotoIf(IsUndefined(method), if_null_or_undefined);
  GotoIf(IsNull(method), if_null_or_undefined);

  return method;
}

void CodeStubAssembler::LoadPropertyFromFastObject(Node* object, Node* map,
                                                   Node* descriptors,
                                                   Node* name_index,
                                                   Variable* var_details,
                                                   Variable* var_value) {
  DCHECK_EQ(MachineRepresentation::kWord32, var_details->rep());
  DCHECK_EQ(MachineRepresentation::kTagged, var_value->rep());
  Comment("[ LoadPropertyFromFastObject");

  Node* details =
      LoadDetailsByKeyIndex<DescriptorArray>(descriptors, name_index);
  var_details->Bind(details);

  Node* location = DecodeWord32<PropertyDetails::LocationField>(details);

  Label if_in_field(this), if_in_descriptor(this), done(this);
  Branch(Word32Equal(location, Int32Constant(kField)), &if_in_field,
         &if_in_descriptor);
  BIND(&if_in_field);
  {
    Node* field_index =
        DecodeWordFromWord32<PropertyDetails::FieldIndexField>(details);
    Node* representation =
        DecodeWord32<PropertyDetails::RepresentationField>(details);

    field_index =
        IntPtrAdd(field_index, LoadMapInobjectPropertiesStartInWords(map));
    Node* instance_size_in_words = LoadMapInstanceSizeInWords(map);

    Label if_inobject(this), if_backing_store(this);
    VARIABLE(var_double_value, MachineRepresentation::kFloat64);
    Label rebox_double(this, &var_double_value);
    Branch(UintPtrLessThan(field_index, instance_size_in_words), &if_inobject,
           &if_backing_store);
    BIND(&if_inobject);
    {
      Comment("if_inobject");
      Node* field_offset = TimesPointerSize(field_index);

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
          Node* mutable_heap_number = LoadObjectField(object, field_offset);
          var_double_value.Bind(LoadHeapNumberValue(mutable_heap_number));
        }
        Goto(&rebox_double);
      }
    }
    BIND(&if_backing_store);
    {
      Comment("if_backing_store");
      Node* properties = LoadFastProperties(object);
      field_index = IntPtrSub(field_index, instance_size_in_words);
      Node* value = LoadFixedArrayElement(properties, field_index);

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
        var_double_value.Bind(LoadHeapNumberValue(value));
        Goto(&rebox_double);
      }
    }
    BIND(&rebox_double);
    {
      Comment("rebox_double");
      Node* heap_number = AllocateHeapNumberWithValue(var_double_value.value());
      var_value->Bind(heap_number);
      Goto(&done);
    }
  }
  BIND(&if_in_descriptor);
  {
    var_value->Bind(
        LoadValueByKeyIndex<DescriptorArray>(descriptors, name_index));
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
  CSA_ASSERT(this, IsDictionary(dictionary));

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
  CSA_ASSERT(this, IsDictionary(dictionary));

  Node* property_cell = LoadFixedArrayElement(dictionary, name_index);
  CSA_ASSERT(this, IsPropertyCell(property_cell));

  Node* value = LoadObjectField(property_cell, PropertyCell::kValueOffset);
  GotoIf(WordEqual(value, TheHoleConstant()), if_deleted);

  var_value->Bind(value);

  Node* details = LoadAndUntagToWord32ObjectField(property_cell,
                                                  PropertyCell::kDetailsOffset);
  var_details->Bind(details);

  Comment("] LoadPropertyFromGlobalDictionary");
}

// |value| is the property backing store's contents, which is either a value
// or an accessor pair, as specified by |details|.
// Returns either the original value, or the result of the getter call.
Node* CodeStubAssembler::CallGetterIfAccessor(Node* value, Node* details,
                                              Node* context, Node* receiver,
                                              Label* if_bailout,
                                              GetOwnPropertyMode mode) {
  VARIABLE(var_value, MachineRepresentation::kTagged, value);
  Label done(this), if_accessor_info(this, Label::kDeferred);

  Node* kind = DecodeWord32<PropertyDetails::KindField>(details);
  GotoIf(Word32Equal(kind, Int32Constant(kData)), &done);

  // Accessor case.
  GotoIfNot(IsAccessorPair(value), &if_accessor_info);

  // AccessorPair case.
  {
    if (mode == kCallJSGetter) {
      Node* accessor_pair = value;
      Node* getter =
          LoadObjectField(accessor_pair, AccessorPair::kGetterOffset);
      Node* getter_map = LoadMap(getter);
      Node* instance_type = LoadMapInstanceType(getter_map);
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
    Label if_array(this), if_function(this), if_value(this);

    // Dispatch based on {receiver} instance type.
    Node* receiver_map = LoadMap(receiver);
    Node* receiver_instance_type = LoadMapInstanceType(receiver_map);
    GotoIf(IsJSArrayInstanceType(receiver_instance_type), &if_array);
    GotoIf(IsJSFunctionInstanceType(receiver_instance_type), &if_function);
    Branch(IsJSValueInstanceType(receiver_instance_type), &if_value,
           if_bailout);

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

      // if (!(has_prototype_slot() && !has_non_instance_prototype())) use
      // generic property loading mechanism.
      int has_prototype_slot_mask = 1 << Map::kHasPrototypeSlot;
      int has_non_instance_prototype_mask = 1 << Map::kHasNonInstancePrototype;
      GotoIfNot(
          Word32Equal(Word32And(LoadMapBitField(receiver_map),
                                Int32Constant(has_prototype_slot_mask |
                                              has_non_instance_prototype_mask)),
                      Int32Constant(has_prototype_slot_mask)),
          if_bailout);
      var_value.Bind(LoadJSFunctionPrototype(receiver, if_bailout));
      Goto(&done);
    }

    // JSValue AccessorInfo case.
    BIND(&if_value);
    {
      // We only deal with the "length" accessor on JSValue string wrappers.
      GotoIfNot(IsLengthString(
                    LoadObjectField(accessor_info, AccessorInfo::kNameOffset)),
                if_bailout);
      Node* receiver_value = LoadJSValueValue(receiver);
      GotoIfNot(TaggedIsNotSmi(receiver_value), if_bailout);
      GotoIfNot(IsString(receiver_value), if_bailout);
      var_value.Bind(LoadStringLengthAsSmi(receiver_value));
      Goto(&done);
    }
  }

  BIND(&done);
  return var_value.value();
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

  VARIABLE(var_meta_storage, MachineRepresentation::kTagged);
  VARIABLE(var_entry, MachineType::PointerRepresentation());

  Label if_found_fast(this), if_found_dict(this), if_found_global(this);

  VARIABLE(local_var_details, MachineRepresentation::kWord32);
  if (!var_details) {
    var_details = &local_var_details;
  }
  Variable* vars[] = {var_value, var_details};
  Label if_found(this, 2, vars);

  TryLookupProperty(object, map, instance_type, unique_name, &if_found_fast,
                    &if_found_dict, &if_found_global, &var_meta_storage,
                    &var_entry, if_not_found, if_bailout);
  BIND(&if_found_fast);
  {
    Node* descriptors = var_meta_storage.value();
    Node* name_index = var_entry.value();

    LoadPropertyFromFastObject(object, map, descriptors, name_index,
                               var_details, var_value);
    Goto(&if_found);
  }
  BIND(&if_found_dict);
  {
    Node* dictionary = var_meta_storage.value();
    Node* entry = var_entry.value();
    LoadPropertyFromNameDictionary(dictionary, entry, var_details, var_value);
    Goto(&if_found);
  }
  BIND(&if_found_global);
  {
    Node* dictionary = var_meta_storage.value();
    Node* entry = var_entry.value();

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
    Node* value = CallGetterIfAccessor(var_value->value(), var_details->value(),
                                       context, receiver, if_bailout, mode);
    var_value->Bind(value);
    Goto(if_found_value);
  }
}

void CodeStubAssembler::TryLookupElement(Node* object, Node* map,
                                         Node* instance_type,
                                         Node* intptr_index, Label* if_found,
                                         Label* if_absent, Label* if_not_found,
                                         Label* if_bailout) {
  // Handle special objects in runtime.
  GotoIf(Int32LessThanOrEqual(instance_type,
                              Int32Constant(LAST_SPECIAL_RECEIVER_TYPE)),
         if_bailout);

  Node* elements_kind = LoadMapElementsKind(map);

  // TODO(verwaest): Support other elements kinds as well.
  Label if_isobjectorsmi(this), if_isdouble(this), if_isdictionary(this),
      if_isfaststringwrapper(this), if_isslowstringwrapper(this), if_oob(this),
      if_typedarray(this);
  // clang-format off
  int32_t values[] = {
      // Handled by {if_isobjectorsmi}.
      PACKED_SMI_ELEMENTS, HOLEY_SMI_ELEMENTS, PACKED_ELEMENTS,
          HOLEY_ELEMENTS,
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
  };
  Label* labels[] = {
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
  };
  // clang-format on
  STATIC_ASSERT(arraysize(values) == arraysize(labels));
  Switch(elements_kind, if_bailout, values, labels, arraysize(values));

  BIND(&if_isobjectorsmi);
  {
    Node* elements = LoadElements(object);
    Node* length = LoadAndUntagFixedArrayBaseLength(elements);

    GotoIfNot(UintPtrLessThan(intptr_index, length), &if_oob);

    Node* element = LoadFixedArrayElement(elements, intptr_index);
    Node* the_hole = TheHoleConstant();
    Branch(WordEqual(element, the_hole), if_not_found, if_found);
  }
  BIND(&if_isdouble);
  {
    Node* elements = LoadElements(object);
    Node* length = LoadAndUntagFixedArrayBaseLength(elements);

    GotoIfNot(UintPtrLessThan(intptr_index, length), &if_oob);

    // Check if the element is a double hole, but don't load it.
    LoadFixedDoubleArrayElement(elements, intptr_index, MachineType::None(), 0,
                                INTPTR_PARAMETERS, if_not_found);
    Goto(if_found);
  }
  BIND(&if_isdictionary);
  {
    // Negative keys must be converted to property names.
    GotoIf(IntPtrLessThan(intptr_index, IntPtrConstant(0)), if_bailout);

    VARIABLE(var_entry, MachineType::PointerRepresentation());
    Node* elements = LoadElements(object);
    NumberDictionaryLookup(elements, intptr_index, if_found, &var_entry,
                           if_not_found);
  }
  BIND(&if_isfaststringwrapper);
  {
    CSA_ASSERT(this, HasInstanceType(object, JS_VALUE_TYPE));
    Node* string = LoadJSValueValue(object);
    CSA_ASSERT(this, IsString(string));
    Node* length = LoadStringLengthAsWord(string);
    GotoIf(UintPtrLessThan(intptr_index, length), if_found);
    Goto(&if_isobjectorsmi);
  }
  BIND(&if_isslowstringwrapper);
  {
    CSA_ASSERT(this, HasInstanceType(object, JS_VALUE_TYPE));
    Node* string = LoadJSValueValue(object);
    CSA_ASSERT(this, IsString(string));
    Node* length = LoadStringLengthAsWord(string);
    GotoIf(UintPtrLessThan(intptr_index, length), if_found);
    Goto(&if_isdictionary);
  }
  BIND(&if_typedarray);
  {
    Node* buffer = LoadObjectField(object, JSArrayBufferView::kBufferOffset);
    GotoIf(IsDetachedBuffer(buffer), if_absent);

    Node* length = TryToIntptr(
        LoadObjectField(object, JSTypedArray::kLengthOffset), if_bailout);
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

void CodeStubAssembler::TryPrototypeChainLookup(
    Node* receiver, Node* key, const LookupInHolder& lookup_property_in_holder,
    const LookupInHolder& lookup_element_in_holder, Label* if_end,
    Label* if_bailout, Label* if_proxy) {
  // Ensure receiver is JSReceiver, otherwise bailout.
  Label if_objectisnotsmi(this);
  Branch(TaggedIsSmi(receiver), if_bailout, &if_objectisnotsmi);
  BIND(&if_objectisnotsmi);

  Node* map = LoadMap(receiver);
  Node* instance_type = LoadMapInstanceType(map);
  {
    Label if_objectisreceiver(this);
    STATIC_ASSERT(LAST_JS_RECEIVER_TYPE == LAST_TYPE);
    STATIC_ASSERT(FIRST_JS_RECEIVER_TYPE == JS_PROXY_TYPE);
    Branch(IsJSReceiverInstanceType(instance_type), &if_objectisreceiver,
           if_bailout);
    BIND(&if_objectisreceiver);

    if (if_proxy) {
      GotoIf(InstanceTypeEqual(instance_type, JS_PROXY_TYPE), if_proxy);
    }
  }

  VARIABLE(var_index, MachineType::PointerRepresentation());
  VARIABLE(var_unique, MachineRepresentation::kTagged);

  Label if_keyisindex(this), if_iskeyunique(this);
  TryToName(key, &if_keyisindex, &var_index, &if_iskeyunique, &var_unique,
            if_bailout);

  BIND(&if_iskeyunique);
  {
    VARIABLE(var_holder, MachineRepresentation::kTagged, receiver);
    VARIABLE(var_holder_map, MachineRepresentation::kTagged, map);
    VARIABLE(var_holder_instance_type, MachineRepresentation::kWord32,
             instance_type);

    Variable* merged_variables[] = {&var_holder, &var_holder_map,
                                    &var_holder_instance_type};
    Label loop(this, arraysize(merged_variables), merged_variables);
    Goto(&loop);
    BIND(&loop);
    {
      Node* holder_map = var_holder_map.value();
      Node* holder_instance_type = var_holder_instance_type.value();

      Label next_proto(this);
      lookup_property_in_holder(receiver, var_holder.value(), holder_map,
                                holder_instance_type, var_unique.value(),
                                &next_proto, if_bailout);
      BIND(&next_proto);

      // Bailout if it can be an integer indexed exotic case.
      GotoIf(InstanceTypeEqual(holder_instance_type, JS_TYPED_ARRAY_TYPE),
             if_bailout);

      Node* proto = LoadMapPrototype(holder_map);

      GotoIf(IsNull(proto), if_end);

      Node* map = LoadMap(proto);
      Node* instance_type = LoadMapInstanceType(map);

      var_holder.Bind(proto);
      var_holder_map.Bind(map);
      var_holder_instance_type.Bind(instance_type);
      Goto(&loop);
    }
  }
  BIND(&if_keyisindex);
  {
    VARIABLE(var_holder, MachineRepresentation::kTagged, receiver);
    VARIABLE(var_holder_map, MachineRepresentation::kTagged, map);
    VARIABLE(var_holder_instance_type, MachineRepresentation::kWord32,
             instance_type);

    Variable* merged_variables[] = {&var_holder, &var_holder_map,
                                    &var_holder_instance_type};
    Label loop(this, arraysize(merged_variables), merged_variables);
    Goto(&loop);
    BIND(&loop);
    {
      Label next_proto(this);
      lookup_element_in_holder(receiver, var_holder.value(),
                               var_holder_map.value(),
                               var_holder_instance_type.value(),
                               var_index.value(), &next_proto, if_bailout);
      BIND(&next_proto);

      Node* proto = LoadMapPrototype(var_holder_map.value());

      GotoIf(IsNull(proto), if_end);

      Node* map = LoadMap(proto);
      Node* instance_type = LoadMapInstanceType(map);

      var_holder.Bind(proto);
      var_holder_map.Bind(map);
      var_holder_instance_type.Bind(instance_type);
      Goto(&loop);
    }
  }
}

Node* CodeStubAssembler::HasInPrototypeChain(Node* context, Node* object,
                                             Node* prototype) {
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
    Node* object_instance_type = LoadMapInstanceType(object_map);
    Branch(IsSpecialReceiverInstanceType(object_instance_type),
           &if_objectisspecial, &if_objectisdirect);
    BIND(&if_objectisspecial);
    {
      // The {object_map} is a special receiver map or a primitive map, check
      // if we need to use the if_objectisspecial path in the runtime.
      GotoIf(InstanceTypeEqual(object_instance_type, JS_PROXY_TYPE),
             &return_runtime);
      Node* object_bitfield = LoadMapBitField(object_map);
      int mask =
          1 << Map::kHasNamedInterceptor | 1 << Map::kIsAccessCheckNeeded;
      Branch(IsSetWord32(object_bitfield, mask), &return_runtime,
             &if_objectisdirect);
    }
    BIND(&if_objectisdirect);

    // Check the current {object} prototype.
    Node* object_prototype = LoadMapPrototype(object_map);
    GotoIf(IsNull(object_prototype), &return_false);
    GotoIf(WordEqual(object_prototype, prototype), &return_true);

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

  // Goto runtime if {object} is a Smi.
  GotoIf(TaggedIsSmi(object), &return_runtime);

  // Goto runtime if {callable} is a Smi.
  GotoIf(TaggedIsSmi(callable), &return_runtime);

  // Load map of {callable}.
  Node* callable_map = LoadMap(callable);

  // Goto runtime if {callable} is not a JSFunction.
  Node* callable_instance_type = LoadMapInstanceType(callable_map);
  GotoIfNot(InstanceTypeEqual(callable_instance_type, JS_FUNCTION_TYPE),
            &return_runtime);

  // Goto runtime if {callable} is not a constructor or has
  // a non-instance "prototype".
  Node* callable_bitfield = LoadMapBitField(callable_map);
  GotoIfNot(
      Word32Equal(Word32And(callable_bitfield,
                            Int32Constant((1 << Map::kHasNonInstancePrototype) |
                                          (1 << Map::kIsConstructor))),
                  Int32Constant(1 << Map::kIsConstructor)),
      &return_runtime);

  // Get the "prototype" (or initial map) of the {callable}.
  Node* callable_prototype =
      LoadObjectField(callable, JSFunction::kPrototypeOrInitialMapOffset);
  {
    Label callable_prototype_valid(this);
    VARIABLE(var_callable_prototype, MachineRepresentation::kTagged,
             callable_prototype);

    // Resolve the "prototype" if the {callable} has an initial map.  Afterwards
    // the {callable_prototype} will be either the JSReceiver prototype object
    // or the hole value, which means that no instances of the {callable} were
    // created so far and hence we should return false.
    Node* callable_prototype_instance_type =
        LoadInstanceType(callable_prototype);
    GotoIfNot(InstanceTypeEqual(callable_prototype_instance_type, MAP_TYPE),
              &callable_prototype_valid);
    var_callable_prototype.Bind(
        LoadObjectField(callable_prototype, Map::kPrototypeOffset));
    Goto(&callable_prototype_valid);
    BIND(&callable_prototype_valid);
    callable_prototype = var_callable_prototype.value();
  }

  // Loop through the prototype chain looking for the {callable} prototype.
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

Node* CodeStubAssembler::ElementOffsetFromIndex(Node* index_node,
                                                ElementsKind kind,
                                                ParameterMode mode,
                                                int base_size) {
  int element_size_shift = ElementsKindToShiftSize(kind);
  int element_size = 1 << element_size_shift;
  int const kSmiShiftBits = kSmiShiftSize + kSmiTagSize;
  intptr_t index = 0;
  bool constant_index = false;
  if (mode == SMI_PARAMETERS) {
    element_size_shift -= kSmiShiftBits;
    Smi* smi_index;
    constant_index = ToSmiConstant(index_node, smi_index);
    if (constant_index) index = smi_index->value();
    index_node = BitcastTaggedToWord(index_node);
  } else {
    DCHECK(mode == INTPTR_PARAMETERS);
    constant_index = ToIntPtrConstant(index_node, index);
  }
  if (constant_index) {
    return IntPtrConstant(base_size + element_size * index);
  }

  Node* shifted_index =
      (element_size_shift == 0)
          ? index_node
          : ((element_size_shift > 0)
                 ? WordShl(index_node, IntPtrConstant(element_size_shift))
                 : WordShr(index_node, IntPtrConstant(-element_size_shift)));
  return IntPtrAdd(IntPtrConstant(base_size), shifted_index);
}

Node* CodeStubAssembler::LoadFeedbackVector(Node* closure) {
  Node* cell = LoadObjectField(closure, JSFunction::kFeedbackVectorOffset);
  return LoadObjectField(cell, Cell::kValueOffset);
}

Node* CodeStubAssembler::LoadFeedbackVectorForStub() {
  Node* function =
      LoadFromParentFrame(JavaScriptFrameConstants::kFunctionOffset);
  return LoadFeedbackVector(function);
}

void CodeStubAssembler::UpdateFeedback(Node* feedback, Node* feedback_vector,
                                       Node* slot_id) {
  // This method is used for binary op and compare feedback. These
  // vector nodes are initialized with a smi 0, so we can simply OR
  // our new feedback in place.
  Node* previous_feedback = LoadFeedbackVectorSlot(feedback_vector, slot_id);
  Node* combined_feedback = SmiOr(previous_feedback, feedback);
  Label end(this);

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

void CodeStubAssembler::CombineFeedback(Variable* existing_feedback,
                                        Node* feedback) {
  existing_feedback->Bind(SmiOr(existing_feedback->value(), feedback));
}

void CodeStubAssembler::CheckForAssociatedProtector(Node* name,
                                                    Label* if_protector) {
  // This list must be kept in sync with LookupIterator::UpdateProtector!
  // TODO(jkummerow): Would it be faster to have a bit in Symbol::flags()?
  GotoIf(WordEqual(name, LoadRoot(Heap::kconstructor_stringRootIndex)),
         if_protector);
  GotoIf(WordEqual(name, LoadRoot(Heap::kiterator_symbolRootIndex)),
         if_protector);
  GotoIf(WordEqual(name, LoadRoot(Heap::kspecies_symbolRootIndex)),
         if_protector);
  GotoIf(WordEqual(name, LoadRoot(Heap::kis_concat_spreadable_symbolRootIndex)),
         if_protector);
  // Fall through if no case matched.
}

Node* CodeStubAssembler::LoadReceiverMap(Node* receiver) {
  return Select(TaggedIsSmi(receiver),
                [=] { return LoadRoot(Heap::kHeapNumberMapRootIndex); },
                [=] { return LoadMap(receiver); },
                MachineRepresentation::kTagged);
}

Node* CodeStubAssembler::TryToIntptr(Node* key, Label* miss) {
  VARIABLE(var_intptr_key, MachineType::PointerRepresentation());
  Label done(this, &var_intptr_key), key_is_smi(this);
  GotoIf(TaggedIsSmi(key), &key_is_smi);
  // Try to convert a heap number to a Smi.
  GotoIfNot(IsHeapNumber(key), miss);
  {
    Node* value = LoadHeapNumberValue(key);
    Node* int_value = RoundFloat64ToInt32(value);
    GotoIfNot(Float64Equal(value, ChangeInt32ToFloat64(int_value)), miss);
    var_intptr_key.Bind(ChangeInt32ToIntPtr(int_value));
    Goto(&done);
  }

  BIND(&key_is_smi);
  {
    var_intptr_key.Bind(SmiUntag(key));
    Goto(&done);
  }

  BIND(&done);
  return var_intptr_key.value();
}

Node* CodeStubAssembler::EmitKeyedSloppyArguments(Node* receiver, Node* key,
                                                  Node* value, Label* bailout) {
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

  bool is_load = value == nullptr;

  GotoIfNot(TaggedIsSmi(key), bailout);
  key = SmiUntag(key);
  GotoIf(IntPtrLessThan(key, IntPtrConstant(0)), bailout);

  Node* elements = LoadElements(receiver);
  Node* elements_length = LoadAndUntagFixedArrayBaseLength(elements);

  VARIABLE(var_result, MachineRepresentation::kTagged);
  if (!is_load) {
    var_result.Bind(value);
  }
  Label if_mapped(this), if_unmapped(this), end(this, &var_result);
  Node* intptr_two = IntPtrConstant(2);
  Node* adjusted_length = IntPtrSub(elements_length, intptr_two);

  GotoIf(UintPtrGreaterThanOrEqual(key, adjusted_length), &if_unmapped);

  Node* mapped_index =
      LoadFixedArrayElement(elements, IntPtrAdd(key, intptr_two));
  Branch(WordEqual(mapped_index, TheHoleConstant()), &if_unmapped, &if_mapped);

  BIND(&if_mapped);
  {
    CSA_ASSERT(this, TaggedIsSmi(mapped_index));
    mapped_index = SmiUntag(mapped_index);
    Node* the_context = LoadFixedArrayElement(elements, 0);
    // Assert that we can use LoadFixedArrayElement/StoreFixedArrayElement
    // methods for accessing Context.
    STATIC_ASSERT(Context::kHeaderSize == FixedArray::kHeaderSize);
    DCHECK_EQ(Context::SlotOffset(0) + kHeapObjectTag,
              FixedArray::OffsetOfElementAt(0));
    if (is_load) {
      Node* result = LoadFixedArrayElement(the_context, mapped_index);
      CSA_ASSERT(this, WordNotEqual(result, TheHoleConstant()));
      var_result.Bind(result);
    } else {
      StoreFixedArrayElement(the_context, mapped_index, value);
    }
    Goto(&end);
  }

  BIND(&if_unmapped);
  {
    Node* backing_store = LoadFixedArrayElement(elements, 1);
    GotoIf(WordNotEqual(LoadMap(backing_store), FixedArrayMapConstant()),
           bailout);

    Node* backing_store_length =
        LoadAndUntagFixedArrayBaseLength(backing_store);
    GotoIf(UintPtrGreaterThanOrEqual(key, backing_store_length), bailout);

    // The key falls into unmapped range.
    if (is_load) {
      Node* result = LoadFixedArrayElement(backing_store, key);
      GotoIf(WordEqual(result, TheHoleConstant()), bailout);
      var_result.Bind(result);
    } else {
      StoreFixedArrayElement(backing_store, key, value);
    }
    Goto(&end);
  }

  BIND(&end);
  return var_result.value();
}

Node* CodeStubAssembler::LoadScriptContext(Node* context, int context_index) {
  Node* native_context = LoadNativeContext(context);
  Node* script_context_table =
      LoadContextElement(native_context, Context::SCRIPT_CONTEXT_TABLE_INDEX);

  int offset =
      ScriptContextTable::GetContextOffset(context_index) - kHeapObjectTag;
  return Load(MachineType::AnyTagged(), script_context_table,
              IntPtrConstant(offset));
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
  if (IsFixedTypedArrayElementsKind(kind)) {
    if (kind == UINT8_CLAMPED_ELEMENTS) {
      CSA_ASSERT(this,
                 Word32Equal(value, Word32And(Int32Constant(0xff), value)));
    }
    Node* offset = ElementOffsetFromIndex(index, kind, mode, 0);
    MachineRepresentation rep = ElementsKindToMachineRepresentation(kind);
    StoreNoWriteBarrier(rep, elements, offset, value);
    return;
  }

  WriteBarrierMode barrier_mode =
      IsSmiElementsKind(kind) ? SKIP_WRITE_BARRIER : UPDATE_WRITE_BARRIER;
  if (IsDoubleElementsKind(kind)) {
    // Make sure we do not store signalling NaNs into double arrays.
    value = Float64SilenceNaN(value);
    StoreFixedDoubleArrayElement(elements, index, value, mode);
  } else {
    StoreFixedArrayElement(elements, index, value, barrier_mode, 0, mode);
  }
}

Node* CodeStubAssembler::Int32ToUint8Clamped(Node* int32_value) {
  Label done(this);
  Node* int32_zero = Int32Constant(0);
  Node* int32_255 = Int32Constant(255);
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
    Node* rounded_value = Float64RoundToEven(float64_value);
    var_value.Bind(TruncateFloat64ToWord32(rounded_value));
    Goto(&done);
  }
  BIND(&done);
  return var_value.value();
}

Node* CodeStubAssembler::PrepareValueForWriteToTypedArray(
    Node* input, ElementsKind elements_kind, Label* bailout) {
  DCHECK(IsFixedTypedArrayElementsKind(elements_kind));

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
    default:
      UNREACHABLE();
  }

  VARIABLE(var_result, rep);
  Label done(this, &var_result), if_smi(this), if_heapnumber(this);
  GotoIf(TaggedIsSmi(input), &if_smi);
  // We can handle both HeapNumber and Oddball here, since Oddball has the
  // same layout as the HeapNumber for the HeapNumber::value field. This
  // way we can also properly optimize stores of oddballs to typed arrays.
  GotoIf(IsHeapNumber(input), &if_heapnumber);
  Branch(HasInstanceType(input, ODDBALL_TYPE), &if_heapnumber, bailout);

  BIND(&if_heapnumber);
  {
    Node* value = LoadHeapNumberValue(input);
    if (rep == MachineRepresentation::kWord32) {
      if (elements_kind == UINT8_CLAMPED_ELEMENTS) {
        value = Float64ToUint8Clamped(value);
      } else {
        value = TruncateFloat64ToWord32(value);
      }
    } else if (rep == MachineRepresentation::kFloat32) {
      value = TruncateFloat64ToFloat32(value);
    } else {
      DCHECK_EQ(MachineRepresentation::kFloat64, rep);
    }
    var_result.Bind(value);
    Goto(&done);
  }

  BIND(&if_smi);
  {
    Node* value = SmiToWord32(input);
    if (rep == MachineRepresentation::kFloat32) {
      value = RoundInt32ToFloat32(value);
    } else if (rep == MachineRepresentation::kFloat64) {
      value = ChangeInt32ToFloat64(value);
    } else {
      DCHECK_EQ(MachineRepresentation::kWord32, rep);
      if (elements_kind == UINT8_CLAMPED_ELEMENTS) {
        value = Int32ToUint8Clamped(value);
      }
    }
    var_result.Bind(value);
    Goto(&done);
  }

  BIND(&done);
  return var_result.value();
}

void CodeStubAssembler::EmitElementStore(Node* object, Node* key, Node* value,
                                         bool is_jsarray,
                                         ElementsKind elements_kind,
                                         KeyedAccessStoreMode store_mode,
                                         Label* bailout) {
  CSA_ASSERT(this, Word32BinaryNot(IsJSProxy(object)));
  Node* elements = LoadElements(object);
  if (IsSmiOrObjectElementsKind(elements_kind) &&
      store_mode != STORE_NO_TRANSITION_HANDLE_COW) {
    // Bailout in case of COW elements.
    GotoIf(WordNotEqual(LoadMap(elements),
                        LoadRoot(Heap::kFixedArrayMapRootIndex)),
           bailout);
  }
  // TODO(ishell): introduce TryToIntPtrOrSmi() and use OptimalParameterMode().
  ParameterMode parameter_mode = INTPTR_PARAMETERS;
  key = TryToIntptr(key, bailout);

  if (IsFixedTypedArrayElementsKind(elements_kind)) {
    Label done(this);
    // TODO(ishell): call ToNumber() on value and don't bailout but be careful
    // to call it only once if we decide to bailout because of bounds checks.

    value = PrepareValueForWriteToTypedArray(value, elements_kind, bailout);

    // There must be no allocations between the buffer load and
    // and the actual store to backing store, because GC may decide that
    // the buffer is not alive or move the elements.
    // TODO(ishell): introduce DisallowHeapAllocationCode scope here.

    // Check if buffer has been neutered.
    Node* buffer = LoadObjectField(object, JSArrayBufferView::kBufferOffset);
    GotoIf(IsDetachedBuffer(buffer), bailout);

    // Bounds check.
    Node* length = TaggedToParameter(
        CAST(LoadObjectField(object, JSTypedArray::kLengthOffset)),
        parameter_mode);

    if (store_mode == STORE_NO_TRANSITION_IGNORE_OUT_OF_BOUNDS) {
      // Skip the store if we write beyond the length.
      GotoIfNot(IntPtrLessThan(key, length), &done);
      // ... but bailout if the key is negative.
    } else {
      DCHECK_EQ(STANDARD_STORE, store_mode);
    }
    GotoIfNot(UintPtrLessThan(key, length), bailout);

    // Backing store = external_pointer + base_pointer.
    Node* external_pointer =
        LoadObjectField(elements, FixedTypedArrayBase::kExternalPointerOffset,
                        MachineType::Pointer());
    Node* base_pointer =
        LoadObjectField(elements, FixedTypedArrayBase::kBasePointerOffset);
    Node* backing_store =
        IntPtrAdd(external_pointer, BitcastTaggedToWord(base_pointer));
    StoreElement(backing_store, elements_kind, key, value, parameter_mode);
    Goto(&done);

    BIND(&done);
    return;
  }
  DCHECK(IsSmiOrObjectElementsKind(elements_kind) ||
         IsDoubleElementsKind(elements_kind));

  Node* length = is_jsarray ? LoadJSArrayLength(object)
                            : LoadFixedArrayBaseLength(elements);
  length = TaggedToParameter(length, parameter_mode);

  // In case value is stored into a fast smi array, assure that the value is
  // a smi before manipulating the backing store. Otherwise the backing store
  // may be left in an invalid state.
  if (IsSmiElementsKind(elements_kind)) {
    GotoIfNot(TaggedIsSmi(value), bailout);
  } else if (IsDoubleElementsKind(elements_kind)) {
    value = TryTaggedToFloat64(value, bailout);
  }

  if (IsGrowStoreMode(store_mode)) {
    elements = CheckForCapacityGrow(object, elements, elements_kind, length,
                                    key, parameter_mode, is_jsarray, bailout);
  } else {
    GotoIfNot(UintPtrLessThan(key, length), bailout);

    if ((store_mode == STORE_NO_TRANSITION_HANDLE_COW) &&
        IsSmiOrObjectElementsKind(elements_kind)) {
      elements = CopyElementsOnWrite(object, elements, elements_kind, length,
                                     parameter_mode, bailout);
    }
  }
  StoreElement(elements, elements_kind, key, value, parameter_mode);
}

Node* CodeStubAssembler::CheckForCapacityGrow(Node* object, Node* elements,
                                              ElementsKind kind, Node* length,
                                              Node* key, ParameterMode mode,
                                              bool is_js_array,
                                              Label* bailout) {
  VARIABLE(checked_elements, MachineRepresentation::kTagged);
  Label grow_case(this), no_grow_case(this), done(this);

  Node* condition;
  if (IsHoleyOrDictionaryElementsKind(kind)) {
    condition = UintPtrGreaterThanOrEqual(key, length);
  } else {
    condition = WordEqual(key, length);
  }
  Branch(condition, &grow_case, &no_grow_case);

  BIND(&grow_case);
  {
    Node* current_capacity =
        TaggedToParameter(LoadFixedArrayBaseLength(elements), mode);

    checked_elements.Bind(elements);

    Label fits_capacity(this);
    GotoIf(UintPtrLessThan(key, current_capacity), &fits_capacity);
    {
      Node* new_elements = TryGrowElementsCapacity(
          object, elements, kind, key, current_capacity, mode, bailout);

      checked_elements.Bind(new_elements);
      Goto(&fits_capacity);
    }
    BIND(&fits_capacity);

    if (is_js_array) {
      Node* new_length = IntPtrAdd(key, IntPtrOrSmiConstant(1, mode));
      StoreObjectFieldNoWriteBarrier(object, JSArray::kLengthOffset,
                                     ParameterToTagged(new_length, mode));
    }
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

  GotoIfNot(
      WordEqual(LoadMap(elements), LoadRoot(Heap::kFixedCOWArrayMapRootIndex)),
      &done);
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
                                               bool is_jsarray,
                                               Label* bailout) {
  DCHECK(!IsHoleyElementsKind(from_kind) || IsHoleyElementsKind(to_kind));
  if (AllocationSite::ShouldTrack(from_kind, to_kind)) {
    TrapAllocationMemento(object, bailout);
  }

  if (!IsSimpleMapChangeTransition(from_kind, to_kind)) {
    Comment("Non-simple map transition");
    Node* elements = LoadElements(object);

    Label done(this);
    GotoIf(WordEqual(elements, EmptyFixedArrayConstant()), &done);

    // TODO(ishell): Use OptimalParameterMode().
    ParameterMode mode = INTPTR_PARAMETERS;
    Node* elements_length = SmiUntag(LoadFixedArrayBaseLength(elements));
    Node* array_length =
        is_jsarray ? SmiUntag(LoadFastJSArrayLength(object)) : elements_length;

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

  Node* new_space_top_address = ExternalConstant(
      ExternalReference::new_space_allocation_top_address(isolate()));
  const int kMementoMapOffset = JSArray::kSize;
  const int kMementoLastWordOffset =
      kMementoMapOffset + AllocationMemento::kSize - kPointerSize;

  // Bail out if the object is not in new space.
  Node* object_word = BitcastTaggedToWord(object);
  Node* object_page = PageFromAddress(object_word);
  {
    Node* page_flags = Load(MachineType::IntPtr(), object_page,
                            IntPtrConstant(Page::kFlagsOffset));
    GotoIf(WordEqual(WordAnd(page_flags,
                             IntPtrConstant(MemoryChunk::kIsInNewSpaceMask)),
                     IntPtrConstant(0)),
           &no_memento_found);
  }

  Node* memento_last_word = IntPtrAdd(
      object_word, IntPtrConstant(kMementoLastWordOffset - kHeapObjectTag));
  Node* memento_last_word_page = PageFromAddress(memento_last_word);

  Node* new_space_top = Load(MachineType::Pointer(), new_space_top_address);
  Node* new_space_top_page = PageFromAddress(new_space_top);

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
    Node* memento_map = LoadObjectField(object, kMementoMapOffset);
    Branch(
        WordEqual(memento_map, LoadRoot(Heap::kAllocationMementoMapRootIndex)),
        memento_found, &no_memento_found);
  }
  BIND(&no_memento_found);
  Comment("] TrapAllocationMemento");
}

Node* CodeStubAssembler::PageFromAddress(Node* address) {
  return WordAnd(address, IntPtrConstant(~Page::kPageAlignmentMask));
}

Node* CodeStubAssembler::CreateAllocationSiteInFeedbackVector(
    Node* feedback_vector, Node* slot) {
  Node* size = IntPtrConstant(AllocationSite::kSize);
  Node* site = Allocate(size, CodeStubAssembler::kPretenured);
  StoreMapNoWriteBarrier(site, Heap::kAllocationSiteMapRootIndex);
  // Should match AllocationSite::Initialize.
  Node* field = UpdateWord<AllocationSite::ElementsKindBits>(
      IntPtrConstant(0), IntPtrConstant(GetInitialFastElementsKind()));
  StoreObjectFieldNoWriteBarrier(
      site, AllocationSite::kTransitionInfoOrBoilerplateOffset, SmiTag(field));

  // Unlike literals, constructed arrays don't have nested sites
  Node* zero = SmiConstant(0);
  StoreObjectFieldNoWriteBarrier(site, AllocationSite::kNestedSiteOffset, zero);

  // Pretenuring calculation field.
  StoreObjectFieldNoWriteBarrier(site, AllocationSite::kPretenureDataOffset,
                                 zero);

  // Pretenuring memento creation count field.
  StoreObjectFieldNoWriteBarrier(
      site, AllocationSite::kPretenureCreateCountOffset, zero);

  // Store an empty fixed array for the code dependency.
  StoreObjectFieldRoot(site, AllocationSite::kDependentCodeOffset,
                       Heap::kEmptyFixedArrayRootIndex);

  // Link the object to the allocation site list
  Node* site_list = ExternalConstant(
      ExternalReference::allocation_sites_list_address(isolate()));
  Node* next_site = LoadBufferObject(site_list, 0);

  // TODO(mvstanton): This is a store to a weak pointer, which we may want to
  // mark as such in order to skip the write barrier, once we have a unified
  // system for weakness. For now we decided to keep it like this because having
  // an initial write barrier backed store makes this pointer strong until the
  // next GC, and allocation sites are designed to survive several GCs anyway.
  StoreObjectField(site, AllocationSite::kWeakNextOffset, next_site);
  StoreNoWriteBarrier(MachineRepresentation::kTagged, site_list, site);

  StoreFeedbackVectorSlot(feedback_vector, slot, site, UPDATE_WRITE_BARRIER, 0,
                          CodeStubAssembler::SMI_PARAMETERS);
  return site;
}

Node* CodeStubAssembler::CreateWeakCellInFeedbackVector(Node* feedback_vector,
                                                        Node* slot,
                                                        Node* value) {
  Node* size = IntPtrConstant(WeakCell::kSize);
  Node* cell = Allocate(size, CodeStubAssembler::kPretenured);

  // Initialize the WeakCell.
  DCHECK(Heap::RootIsImmortalImmovable(Heap::kWeakCellMapRootIndex));
  StoreMapNoWriteBarrier(cell, Heap::kWeakCellMapRootIndex);
  StoreObjectField(cell, WeakCell::kValueOffset, value);

  // Store the WeakCell in the feedback vector.
  StoreFeedbackVectorSlot(feedback_vector, slot, cell, UPDATE_WRITE_BARRIER, 0,
                          CodeStubAssembler::INTPTR_PARAMETERS);
  return cell;
}

Node* CodeStubAssembler::BuildFastLoop(
    const CodeStubAssembler::VariableList& vars, Node* start_index,
    Node* end_index, const FastLoopBody& body, int increment,
    ParameterMode parameter_mode, IndexAdvanceMode advance_mode) {
  CSA_SLOW_ASSERT(this, MatchesParameterMode(start_index, parameter_mode));
  CSA_SLOW_ASSERT(this, MatchesParameterMode(end_index, parameter_mode));
  MachineRepresentation index_rep = (parameter_mode == INTPTR_PARAMETERS)
                                        ? MachineType::PointerRepresentation()
                                        : MachineRepresentation::kTaggedSigned;
  VARIABLE(var, index_rep, start_index);
  VariableList vars_copy(vars, zone());
  vars_copy.Add(&var, zone());
  Label loop(this, vars_copy);
  Label after_loop(this);
  // Introduce an explicit second check of the termination condition before the
  // loop that helps turbofan generate better code. If there's only a single
  // check, then the CodeStubAssembler forces it to be at the beginning of the
  // loop requiring a backwards branch at the end of the loop (it's not possible
  // to force the loop header check at the end of the loop and branch forward to
  // it from the pre-header). The extra branch is slower in the case that the
  // loop actually iterates.
  Branch(WordEqual(var.value(), end_index), &after_loop, &loop);
  BIND(&loop);
  {
    if (advance_mode == IndexAdvanceMode::kPre) {
      Increment(&var, increment, parameter_mode);
    }
    body(var.value());
    if (advance_mode == IndexAdvanceMode::kPost) {
      Increment(&var, increment, parameter_mode);
    }
    Branch(WordNotEqual(var.value(), end_index), &loop, &after_loop);
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
  bool constant_first = ToInt32Constant(first_element_inclusive, first_val);
  int32_t last_val;
  bool constent_last = ToInt32Constant(last_element_exclusive, last_val);
  if (constant_first && constent_last) {
    int delta = last_val - first_val;
    DCHECK_GE(delta, 0);
    if (delta <= kElementLoopUnrollThreshold) {
      if (direction == ForEachDirection::kForward) {
        for (int i = first_val; i < last_val; ++i) {
          Node* index = IntPtrConstant(i);
          Node* offset =
              ElementOffsetFromIndex(index, kind, INTPTR_PARAMETERS,
                                     FixedArray::kHeaderSize - kHeapObjectTag);
          body(fixed_array, offset);
        }
      } else {
        for (int i = last_val - 1; i >= first_val; --i) {
          Node* index = IntPtrConstant(i);
          Node* offset =
              ElementOffsetFromIndex(index, kind, INTPTR_PARAMETERS,
                                     FixedArray::kHeaderSize - kHeapObjectTag);
          body(fixed_array, offset);
        }
      }
      return;
    }
  }

  Node* start =
      ElementOffsetFromIndex(first_element_inclusive, kind, mode,
                             FixedArray::kHeaderSize - kHeapObjectTag);
  Node* limit =
      ElementOffsetFromIndex(last_element_exclusive, kind, mode,
                             FixedArray::kHeaderSize - kHeapObjectTag);
  if (direction == ForEachDirection::kReverse) std::swap(start, limit);

  int increment = IsDoubleElementsKind(kind) ? kDoubleSize : kPointerSize;
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

void CodeStubAssembler::InitializeFieldsWithRoot(
    Node* object, Node* start_offset, Node* end_offset,
    Heap::RootListIndex root_index) {
  CSA_SLOW_ASSERT(this, TaggedIsNotSmi(object));
  start_offset = IntPtrAdd(start_offset, IntPtrConstant(-kHeapObjectTag));
  end_offset = IntPtrAdd(end_offset, IntPtrConstant(-kHeapObjectTag));
  Node* root_value = LoadRoot(root_index);
  BuildFastLoop(end_offset, start_offset,
                [this, object, root_value](Node* current) {
                  StoreNoWriteBarrier(MachineRepresentation::kTagged, object,
                                      current, root_value);
                },
                -kPointerSize, INTPTR_PARAMETERS,
                CodeStubAssembler::IndexAdvanceMode::kPre);
}

void CodeStubAssembler::BranchIfNumericRelationalComparison(
    Operation op, Node* lhs, Node* rhs, Label* if_true, Label* if_false) {
  CSA_SLOW_ASSERT(this, IsNumber(lhs));
  CSA_SLOW_ASSERT(this, IsNumber(rhs));

  Label end(this);
  VARIABLE(result, MachineRepresentation::kTagged);

  // Shared entry for floating point comparison.
  Label do_fcmp(this);
  VARIABLE(var_fcmp_lhs, MachineRepresentation::kFloat64);
  VARIABLE(var_fcmp_rhs, MachineRepresentation::kFloat64);

  // Check if the {lhs} is a Smi or a HeapObject.
  Label if_lhsissmi(this), if_lhsisnotsmi(this);
  Branch(TaggedIsSmi(lhs), &if_lhsissmi, &if_lhsisnotsmi);

  BIND(&if_lhsissmi);
  {
    // Check if {rhs} is a Smi or a HeapObject.
    Label if_rhsissmi(this), if_rhsisnotsmi(this);
    Branch(TaggedIsSmi(rhs), &if_rhsissmi, &if_rhsisnotsmi);

    BIND(&if_rhsissmi);
    {
      // Both {lhs} and {rhs} are Smi, so just perform a fast Smi comparison.
      switch (op) {
        case Operation::kLessThan:
          BranchIfSmiLessThan(lhs, rhs, if_true, if_false);
          break;
        case Operation::kLessThanOrEqual:
          BranchIfSmiLessThanOrEqual(lhs, rhs, if_true, if_false);
          break;
        case Operation::kGreaterThan:
          BranchIfSmiLessThan(rhs, lhs, if_true, if_false);
          break;
        case Operation::kGreaterThanOrEqual:
          BranchIfSmiLessThanOrEqual(rhs, lhs, if_true, if_false);
          break;
        default:
          UNREACHABLE();
      }
    }

    BIND(&if_rhsisnotsmi);
    {
      CSA_ASSERT(this, IsHeapNumber(rhs));
      // Convert the {lhs} and {rhs} to floating point values, and
      // perform a floating point comparison.
      var_fcmp_lhs.Bind(SmiToFloat64(lhs));
      var_fcmp_rhs.Bind(LoadHeapNumberValue(rhs));
      Goto(&do_fcmp);
    }
  }

  BIND(&if_lhsisnotsmi);
  {
    CSA_ASSERT(this, IsHeapNumber(lhs));

    // Check if {rhs} is a Smi or a HeapObject.
    Label if_rhsissmi(this), if_rhsisnotsmi(this);
    Branch(TaggedIsSmi(rhs), &if_rhsissmi, &if_rhsisnotsmi);

    BIND(&if_rhsissmi);
    {
      // Convert the {lhs} and {rhs} to floating point values, and
      // perform a floating point comparison.
      var_fcmp_lhs.Bind(LoadHeapNumberValue(lhs));
      var_fcmp_rhs.Bind(SmiToFloat64(rhs));
      Goto(&do_fcmp);
    }

    BIND(&if_rhsisnotsmi);
    {
      CSA_ASSERT(this, IsHeapNumber(rhs));

      // Convert the {lhs} and {rhs} to floating point values, and
      // perform a floating point comparison.
      var_fcmp_lhs.Bind(LoadHeapNumberValue(lhs));
      var_fcmp_rhs.Bind(LoadHeapNumberValue(rhs));
      Goto(&do_fcmp);
    }
  }

  BIND(&do_fcmp);
  {
    // Load the {lhs} and {rhs} floating point values.
    Node* lhs = var_fcmp_lhs.value();
    Node* rhs = var_fcmp_rhs.value();

    // Perform a fast floating point comparison.
    switch (op) {
      case Operation::kLessThan:
        Branch(Float64LessThan(lhs, rhs), if_true, if_false);
        break;
      case Operation::kLessThanOrEqual:
        Branch(Float64LessThanOrEqual(lhs, rhs), if_true, if_false);
        break;
      case Operation::kGreaterThan:
        Branch(Float64GreaterThan(lhs, rhs), if_true, if_false);
        break;
      case Operation::kGreaterThanOrEqual:
        Branch(Float64GreaterThanOrEqual(lhs, rhs), if_true, if_false);
        break;
      default:
        UNREACHABLE();
    }
  }
}

void CodeStubAssembler::GotoIfNumericGreaterThanOrEqual(Node* lhs, Node* rhs,
                                                        Label* if_true) {
  Label if_false(this);
  BranchIfNumericRelationalComparison(Operation::kGreaterThanOrEqual, lhs, rhs,
                                      if_true, &if_false);
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

Node* CodeStubAssembler::RelationalComparison(Operation op, Node* lhs,
                                              Node* rhs, Node* context,
                                              Variable* var_type_feedback) {
  Label return_true(this), return_false(this), end(this);
  VARIABLE(result, MachineRepresentation::kTagged);

  // Shared entry for floating point comparison.
  Label do_fcmp(this);
  VARIABLE(var_fcmp_lhs, MachineRepresentation::kFloat64);
  VARIABLE(var_fcmp_rhs, MachineRepresentation::kFloat64);

  // We might need to loop several times due to ToPrimitive and/or ToNumeric
  // conversions.
  VARIABLE(var_lhs, MachineRepresentation::kTagged, lhs);
  VARIABLE(var_rhs, MachineRepresentation::kTagged, rhs);
  VariableList loop_variable_list({&var_lhs, &var_rhs}, zone());
  if (var_type_feedback != nullptr) {
    // Initialize the type feedback to None. The current feedback is combined
    // with the previous feedback.
    var_type_feedback->Bind(SmiConstant(CompareOperationFeedback::kNone));
    loop_variable_list.Add(var_type_feedback, zone());
  }
  Label loop(this, loop_variable_list);
  Goto(&loop);
  BIND(&loop);
  {
    // Load the current {lhs} and {rhs} values.
    lhs = var_lhs.value();
    rhs = var_rhs.value();

    // Check if the {lhs} is a Smi or a HeapObject.
    Label if_lhsissmi(this), if_lhsisnotsmi(this);
    Branch(TaggedIsSmi(lhs), &if_lhsissmi, &if_lhsisnotsmi);

    BIND(&if_lhsissmi);
    {
      Label if_rhsissmi(this), if_rhsisheapnumber(this),
          if_rhsisbigint(this, Label::kDeferred),
          if_rhsisnotnumeric(this, Label::kDeferred);
      GotoIf(TaggedIsSmi(rhs), &if_rhsissmi);
      Node* rhs_map = LoadMap(rhs);
      GotoIf(IsHeapNumberMap(rhs_map), &if_rhsisheapnumber);
      Node* rhs_instance_type = LoadMapInstanceType(rhs_map);
      Branch(IsBigIntInstanceType(rhs_instance_type), &if_rhsisbigint,
             &if_rhsisnotnumeric);

      BIND(&if_rhsissmi);
      {
        // Both {lhs} and {rhs} are Smi, so just perform a fast Smi comparison.
        if (var_type_feedback != nullptr) {
          CombineFeedback(var_type_feedback,
                          SmiConstant(CompareOperationFeedback::kSignedSmall));
        }
        switch (op) {
          case Operation::kLessThan:
            BranchIfSmiLessThan(lhs, rhs, &return_true, &return_false);
            break;
          case Operation::kLessThanOrEqual:
            BranchIfSmiLessThanOrEqual(lhs, rhs, &return_true, &return_false);
            break;
          case Operation::kGreaterThan:
            BranchIfSmiLessThan(rhs, lhs, &return_true, &return_false);
            break;
          case Operation::kGreaterThanOrEqual:
            BranchIfSmiLessThanOrEqual(rhs, lhs, &return_true, &return_false);
            break;
          default:
            UNREACHABLE();
        }
      }

      BIND(&if_rhsisheapnumber);
      {
        // Convert the {lhs} and {rhs} to floating point values, and
        // perform a floating point comparison.
        if (var_type_feedback != nullptr) {
          CombineFeedback(var_type_feedback,
                          SmiConstant(CompareOperationFeedback::kNumber));
        }
        var_fcmp_lhs.Bind(SmiToFloat64(lhs));
        var_fcmp_rhs.Bind(LoadHeapNumberValue(rhs));
        Goto(&do_fcmp);
      }

      BIND(&if_rhsisbigint);
      {
        // The {lhs} is a Smi and {rhs} is a BigInt.
        if (var_type_feedback != nullptr) {
          var_type_feedback->Bind(SmiConstant(CompareOperationFeedback::kAny));
        }
        result.Bind(CallRuntime(Runtime::kBigIntCompareToNumber,
                                NoContextConstant(), SmiConstant(Reverse(op)),
                                rhs, lhs));
        Goto(&end);
      }

      BIND(&if_rhsisnotnumeric);
      {
        // The {lhs} is a Smi and {rhs} is not a Numeric.
        if (var_type_feedback != nullptr) {
          var_type_feedback->Bind(SmiConstant(CompareOperationFeedback::kAny));
        }
        // Convert the {rhs} to a Numeric; we don't need to perform the
        // dedicated ToPrimitive(rhs, hint Number) operation, as the
        // ToNumeric(rhs) will by itself already invoke ToPrimitive with
        // a Number hint.
        var_rhs.Bind(CallBuiltin(Builtins::kNonNumberToNumeric, context, rhs));
        Goto(&loop);
      }
    }

    BIND(&if_lhsisnotsmi);
    {
      Node* lhs_map = LoadMap(lhs);

      // Check if {rhs} is a Smi or a HeapObject.
      Label if_rhsissmi(this), if_rhsisnotsmi(this);
      Branch(TaggedIsSmi(rhs), &if_rhsissmi, &if_rhsisnotsmi);

      BIND(&if_rhsissmi);
      {
        Label if_lhsisheapnumber(this), if_lhsisbigint(this, Label::kDeferred),
            if_lhsisnotnumeric(this, Label::kDeferred);
        GotoIf(IsHeapNumberMap(lhs_map), &if_lhsisheapnumber);
        Node* lhs_instance_type = LoadMapInstanceType(lhs_map);
        Branch(IsBigIntInstanceType(lhs_instance_type), &if_lhsisbigint,
               &if_lhsisnotnumeric);

        BIND(&if_lhsisheapnumber);
        {
          // Convert the {lhs} and {rhs} to floating point values, and
          // perform a floating point comparison.
          if (var_type_feedback != nullptr) {
            CombineFeedback(var_type_feedback,
                            SmiConstant(CompareOperationFeedback::kNumber));
          }
          var_fcmp_lhs.Bind(LoadHeapNumberValue(lhs));
          var_fcmp_rhs.Bind(SmiToFloat64(rhs));
          Goto(&do_fcmp);
        }

        BIND(&if_lhsisbigint);
        {
          if (var_type_feedback != nullptr) {
            var_type_feedback->Bind(
                SmiConstant(CompareOperationFeedback::kAny));
          }
          result.Bind(CallRuntime(Runtime::kBigIntCompareToNumber,
                                  NoContextConstant(), SmiConstant(op), lhs,
                                  rhs));
          Goto(&end);
        }

        BIND(&if_lhsisnotnumeric);
        {
          // The {lhs} is not a Numeric and {rhs} is an Smi.
          if (var_type_feedback != nullptr) {
            var_type_feedback->Bind(
                SmiConstant(CompareOperationFeedback::kAny));
          }
          // Convert the {lhs} to a Numeric; we don't need to perform the
          // dedicated ToPrimitive(lhs, hint Number) operation, as the
          // ToNumeric(lhs) will by itself already invoke ToPrimitive with
          // a Number hint.
          var_lhs.Bind(
              CallBuiltin(Builtins::kNonNumberToNumeric, context, lhs));
          Goto(&loop);
        }
      }

      BIND(&if_rhsisnotsmi);
      {
        // Load the map of {rhs}.
        Node* rhs_map = LoadMap(rhs);

        // Further analyze {lhs}.
        Label if_lhsisheapnumber(this), if_lhsisbigint(this, Label::kDeferred),
            if_lhsisstring(this), if_lhsisother(this, Label::kDeferred);
        GotoIf(IsHeapNumberMap(lhs_map), &if_lhsisheapnumber);
        Node* lhs_instance_type = LoadMapInstanceType(lhs_map);
        GotoIf(IsBigIntInstanceType(lhs_instance_type), &if_lhsisbigint);
        Branch(IsStringInstanceType(lhs_instance_type), &if_lhsisstring,
               &if_lhsisother);

        BIND(&if_lhsisheapnumber);
        {
          // Further inspect {rhs}.
          Label if_rhsisheapnumber(this),
              if_rhsisbigint(this, Label::kDeferred),
              if_rhsisnotnumeric(this, Label::kDeferred);
          GotoIf(WordEqual(rhs_map, lhs_map), &if_rhsisheapnumber);
          Node* rhs_instance_type = LoadMapInstanceType(rhs_map);
          Branch(IsBigIntInstanceType(rhs_instance_type), &if_rhsisbigint,
                 &if_rhsisnotnumeric);

          BIND(&if_rhsisheapnumber);
          {
            // Convert the {lhs} and {rhs} to floating point values, and
            // perform a floating point comparison.
            if (var_type_feedback != nullptr) {
              CombineFeedback(var_type_feedback,
                              SmiConstant(CompareOperationFeedback::kNumber));
            }
            var_fcmp_lhs.Bind(LoadHeapNumberValue(lhs));
            var_fcmp_rhs.Bind(LoadHeapNumberValue(rhs));
            Goto(&do_fcmp);
          }

          BIND(&if_rhsisbigint);
          {
            if (var_type_feedback != nullptr) {
              var_type_feedback->Bind(
                  SmiConstant(CompareOperationFeedback::kAny));
            }
            result.Bind(CallRuntime(Runtime::kBigIntCompareToNumber,
                                    NoContextConstant(),
                                    SmiConstant(Reverse(op)), rhs, lhs));
            Goto(&end);
          }

          BIND(&if_rhsisnotnumeric);
          {
            // The {lhs} is a HeapNumber and {rhs} is not a Numeric.
            if (var_type_feedback != nullptr) {
              var_type_feedback->Bind(
                  SmiConstant(CompareOperationFeedback::kAny));
            }
            // Convert the {rhs} to a Numeric; we don't need to perform
            // dedicated ToPrimitive(rhs, hint Number) operation, as the
            // ToNumeric(rhs) will by itself already invoke ToPrimitive with
            // a Number hint.
            var_rhs.Bind(
                CallBuiltin(Builtins::kNonNumberToNumeric, context, rhs));
            Goto(&loop);
          }
        }

        BIND(&if_lhsisbigint);
        {
          if (var_type_feedback != nullptr) {
            var_type_feedback->Bind(
                SmiConstant(CompareOperationFeedback::kAny));
          }

          Label if_rhsisheapnumber(this), if_rhsisbigint(this),
              if_rhsisnotnumeric(this);
          GotoIf(IsHeapNumberMap(rhs_map), &if_rhsisheapnumber);
          Node* rhs_instance_type = LoadMapInstanceType(rhs_map);
          Branch(IsBigIntInstanceType(rhs_instance_type), &if_rhsisbigint,
                 &if_rhsisnotnumeric);

          BIND(&if_rhsisheapnumber);
          {
            result.Bind(CallRuntime(Runtime::kBigIntCompareToNumber,
                                    NoContextConstant(), SmiConstant(op), lhs,
                                    rhs));
            Goto(&end);
          }

          BIND(&if_rhsisbigint);
          {
            result.Bind(CallRuntime(Runtime::kBigIntCompareToBigInt,
                                    NoContextConstant(), SmiConstant(op), lhs,
                                    rhs));
            Goto(&end);
          }

          BIND(&if_rhsisnotnumeric);
          {
            // Convert the {rhs} to a Numeric; we don't need to perform
            // dedicated ToPrimitive(rhs, hint Number) operation, as the
            // ToNumeric(rhs) will by itself already invoke ToPrimitive with
            // a Number hint.
            var_rhs.Bind(
                CallBuiltin(Builtins::kNonNumberToNumeric, context, rhs));
            Goto(&loop);
          }
        }

        BIND(&if_lhsisstring);
        {
          // Load the instance type of {rhs}.
          Node* rhs_instance_type = LoadMapInstanceType(rhs_map);

          // Check if {rhs} is also a String.
          Label if_rhsisstring(this, Label::kDeferred),
              if_rhsisnotstring(this, Label::kDeferred);
          Branch(IsStringInstanceType(rhs_instance_type), &if_rhsisstring,
                 &if_rhsisnotstring);

          BIND(&if_rhsisstring);
          {
            // Both {lhs} and {rhs} are strings.
            if (var_type_feedback != nullptr) {
              CombineFeedback(var_type_feedback,
                              SmiConstant(CompareOperationFeedback::kString));
            }
            switch (op) {
              case Operation::kLessThan:
                result.Bind(
                    CallBuiltin(Builtins::kStringLessThan, context, lhs, rhs));
                Goto(&end);
                break;
              case Operation::kLessThanOrEqual:
                result.Bind(CallBuiltin(Builtins::kStringLessThanOrEqual,
                                        context, lhs, rhs));
                Goto(&end);
                break;
              case Operation::kGreaterThan:
                result.Bind(CallBuiltin(Builtins::kStringGreaterThan, context,
                                        lhs, rhs));
                Goto(&end);
                break;
              case Operation::kGreaterThanOrEqual:
                result.Bind(CallBuiltin(Builtins::kStringGreaterThanOrEqual,
                                        context, lhs, rhs));
                Goto(&end);
                break;
              default:
                UNREACHABLE();
            }
          }

          BIND(&if_rhsisnotstring);
          {
            // The {lhs} is a String and {rhs} is not a String.
            if (var_type_feedback != nullptr) {
              var_type_feedback->Bind(
                  SmiConstant(CompareOperationFeedback::kAny));
            }
            // The {lhs} is a String, while {rhs} isn't. So we call
            // ToPrimitive(rhs, hint Number) if {rhs} is a receiver, or
            // ToNumeric(lhs) and then ToNumeric(rhs) in the other cases.
            STATIC_ASSERT(LAST_JS_RECEIVER_TYPE == LAST_TYPE);
            Label if_rhsisreceiver(this, Label::kDeferred),
                if_rhsisnotreceiver(this, Label::kDeferred);
            Branch(IsJSReceiverInstanceType(rhs_instance_type),
                   &if_rhsisreceiver, &if_rhsisnotreceiver);

            BIND(&if_rhsisreceiver);
            {
              // Convert {rhs} to a primitive first passing Number hint.
              Callable callable = CodeFactory::NonPrimitiveToPrimitive(
                  isolate(), ToPrimitiveHint::kNumber);
              var_rhs.Bind(CallStub(callable, context, rhs));
              Goto(&loop);
            }

            BIND(&if_rhsisnotreceiver);
            {
              // Convert both {lhs} and {rhs} to Numeric.
              var_lhs.Bind(
                  CallBuiltin(Builtins::kNonNumberToNumeric, context, lhs));
              var_rhs.Bind(CallBuiltin(Builtins::kToNumeric, context, rhs));
              Goto(&loop);
            }
          }
        }

        BIND(&if_lhsisother);
        {
          // The {lhs} is neither a Numeric nor a String, and {rhs} is not
          // an Smi.
          if (var_type_feedback != nullptr) {
            // Collect NumberOrOddball feedback if {lhs} is an Oddball
            // and {rhs} is either a HeapNumber or Oddball. Otherwise collect
            // Any feedback.
            Label collect_any_feedback(this), collect_oddball_feedback(this),
                collect_feedback_done(this);
            GotoIfNot(InstanceTypeEqual(lhs_instance_type, ODDBALL_TYPE),
                      &collect_any_feedback);

            Node* rhs_instance_type = LoadMapInstanceType(rhs_map);
            GotoIf(InstanceTypeEqual(rhs_instance_type, HEAP_NUMBER_TYPE),
                   &collect_oddball_feedback);
            Branch(InstanceTypeEqual(rhs_instance_type, ODDBALL_TYPE),
                   &collect_oddball_feedback, &collect_any_feedback);

            BIND(&collect_oddball_feedback);
            {
              CombineFeedback(
                  var_type_feedback,
                  SmiConstant(CompareOperationFeedback::kNumberOrOddball));
              Goto(&collect_feedback_done);
            }

            BIND(&collect_any_feedback);
            {
              var_type_feedback->Bind(
                  SmiConstant(CompareOperationFeedback::kAny));
              Goto(&collect_feedback_done);
            }

            BIND(&collect_feedback_done);
          }

          // If {lhs} is a receiver, we must call ToPrimitive(lhs, hint Number).
          // Otherwise we must call ToNumeric(lhs) and then ToNumeric(rhs).
          STATIC_ASSERT(LAST_JS_RECEIVER_TYPE == LAST_TYPE);
          Label if_lhsisreceiver(this, Label::kDeferred),
              if_lhsisnotreceiver(this, Label::kDeferred);
          Branch(IsJSReceiverInstanceType(lhs_instance_type), &if_lhsisreceiver,
                 &if_lhsisnotreceiver);

          BIND(&if_lhsisreceiver);
          {
            Callable callable = CodeFactory::NonPrimitiveToPrimitive(
                isolate(), ToPrimitiveHint::kNumber);
            var_lhs.Bind(CallStub(callable, context, lhs));
            Goto(&loop);
          }

          BIND(&if_lhsisnotreceiver);
          {
            var_lhs.Bind(
                CallBuiltin(Builtins::kNonNumberToNumeric, context, lhs));
            var_rhs.Bind(CallBuiltin(Builtins::kToNumeric, context, rhs));
            Goto(&loop);
          }
        }
      }
    }
  }

  BIND(&do_fcmp);
  {
    // Load the {lhs} and {rhs} floating point values.
    Node* lhs = var_fcmp_lhs.value();
    Node* rhs = var_fcmp_rhs.value();

    // Perform a fast floating point comparison.
    switch (op) {
      case Operation::kLessThan:
        Branch(Float64LessThan(lhs, rhs), &return_true, &return_false);
        break;
      case Operation::kLessThanOrEqual:
        Branch(Float64LessThanOrEqual(lhs, rhs), &return_true, &return_false);
        break;
      case Operation::kGreaterThan:
        Branch(Float64GreaterThan(lhs, rhs), &return_true, &return_false);
        break;
      case Operation::kGreaterThanOrEqual:
        Branch(Float64GreaterThanOrEqual(lhs, rhs), &return_true,
               &return_false);
        break;
      default:
        UNREACHABLE();
    }
  }

  BIND(&return_true);
  {
    result.Bind(TrueConstant());
    Goto(&end);
  }

  BIND(&return_false);
  {
    result.Bind(FalseConstant());
    Goto(&end);
  }

  BIND(&end);
  return result.value();
}

Node* CodeStubAssembler::CollectFeedbackForString(Node* instance_type) {
  Node* feedback = SelectSmiConstant(
      Word32Equal(
          Word32And(instance_type, Int32Constant(kIsNotInternalizedMask)),
          Int32Constant(kInternalizedTag)),
      CompareOperationFeedback::kInternalizedString,
      CompareOperationFeedback::kString);
  return feedback;
}

void CodeStubAssembler::GenerateEqual_Same(Node* value, Label* if_equal,
                                           Label* if_notequal,
                                           Variable* var_type_feedback) {
  // In case of abstract or strict equality checks, we need additional checks
  // for NaN values because they are not considered equal, even if both the
  // left and the right hand side reference exactly the same value.

  Label if_smi(this), if_heapnumber(this);
  GotoIf(TaggedIsSmi(value), &if_smi);

  Node* value_map = LoadMap(value);
  GotoIf(IsHeapNumberMap(value_map), &if_heapnumber);

  // For non-HeapNumbers, all we do is collect type feedback.
  if (var_type_feedback != nullptr) {
    Node* instance_type = LoadMapInstanceType(value_map);

    Label if_string(this), if_receiver(this), if_symbol(this),
        if_other(this, Label::kDeferred);
    GotoIf(IsStringInstanceType(instance_type), &if_string);
    GotoIf(IsJSReceiverInstanceType(instance_type), &if_receiver);
    Branch(IsSymbolInstanceType(instance_type), &if_symbol, &if_other);

    BIND(&if_string);
    {
      CombineFeedback(var_type_feedback,
                      CollectFeedbackForString(instance_type));
      Goto(if_equal);
    }

    BIND(&if_symbol);
    {
      CombineFeedback(var_type_feedback,
                      SmiConstant(CompareOperationFeedback::kSymbol));
      Goto(if_equal);
    }

    BIND(&if_receiver);
    {
      CombineFeedback(var_type_feedback,
                      SmiConstant(CompareOperationFeedback::kReceiver));
      Goto(if_equal);
    }

    // TODO(neis): Introduce BigInt CompareOperationFeedback and collect here
    // and elsewhere?

    BIND(&if_other);
    {
      CombineFeedback(var_type_feedback,
                      SmiConstant(CompareOperationFeedback::kAny));
      Goto(if_equal);
    }
  } else {
    Goto(if_equal);
  }

  BIND(&if_heapnumber);
  {
    if (var_type_feedback != nullptr) {
      CombineFeedback(var_type_feedback,
                      SmiConstant(CompareOperationFeedback::kNumber));
    }
    Node* number_value = LoadHeapNumberValue(value);
    BranchIfFloat64IsNaN(number_value, if_notequal, if_equal);
  }

  BIND(&if_smi);
  {
    if (var_type_feedback != nullptr) {
      CombineFeedback(var_type_feedback,
                      SmiConstant(CompareOperationFeedback::kSignedSmall));
    }
    Goto(if_equal);
  }
}

// ES6 section 7.2.12 Abstract Equality Comparison
Node* CodeStubAssembler::Equal(Node* left, Node* right, Node* context,
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
  VARIABLE(var_left, MachineRepresentation::kTagged, left);
  VARIABLE(var_right, MachineRepresentation::kTagged, right);
  VariableList loop_variable_list({&var_left, &var_right}, zone());
  if (var_type_feedback != nullptr) {
    // Initialize the type feedback to None. The current feedback is combined
    // with the previous feedback.
    var_type_feedback->Bind(SmiConstant(CompareOperationFeedback::kNone));
    loop_variable_list.Add(var_type_feedback, zone());
  }
  Label loop(this, loop_variable_list);
  Goto(&loop);
  BIND(&loop);
  {
    left = var_left.value();
    right = var_right.value();

    Label if_notsame(this);
    GotoIf(WordNotEqual(left, right), &if_notsame);
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
        if (var_type_feedback != nullptr) {
          CombineFeedback(var_type_feedback,
                          SmiConstant(CompareOperationFeedback::kSignedSmall));
        }
        Goto(&if_notequal);
      }

      BIND(&if_right_not_smi);
      Node* right_map = LoadMap(right);
      Label if_right_heapnumber(this), if_right_boolean(this),
          if_right_bigint(this, Label::kDeferred),
          if_right_receiver(this, Label::kDeferred);
      GotoIf(IsHeapNumberMap(right_map), &if_right_heapnumber);
      // {left} is Smi and {right} is not HeapNumber or Smi.
      if (var_type_feedback != nullptr) {
        var_type_feedback->Bind(SmiConstant(CompareOperationFeedback::kAny));
      }
      GotoIf(IsBooleanMap(right_map), &if_right_boolean);
      Node* right_type = LoadMapInstanceType(right_map);
      GotoIf(IsStringInstanceType(right_type), &do_right_stringtonumber);
      GotoIf(IsBigIntInstanceType(right_type), &if_right_bigint);
      Branch(IsJSReceiverInstanceType(right_type), &if_right_receiver,
             &if_notequal);

      BIND(&if_right_heapnumber);
      {
        var_left_float = SmiToFloat64(left);
        var_right_float = LoadHeapNumberValue(right);
        if (var_type_feedback != nullptr) {
          CombineFeedback(var_type_feedback,
                          SmiConstant(CompareOperationFeedback::kNumber));
        }
        Goto(&do_float_comparison);
      }

      BIND(&if_right_boolean);
      {
        var_right.Bind(LoadObjectField(right, Oddball::kToNumberOffset));
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
        var_right.Bind(CallStub(callable, context, right));
        Goto(&loop);
      }
    }

    BIND(&if_left_not_smi);
    {
      GotoIf(TaggedIsSmi(right), &use_symmetry);

      Label if_left_symbol(this), if_left_number(this), if_left_string(this),
          if_left_bigint(this, Label::kDeferred), if_left_oddball(this),
          if_left_receiver(this);

      Node* left_map = LoadMap(left);
      Node* right_map = LoadMap(right);
      Node* left_type = LoadMapInstanceType(left_map);
      Node* right_type = LoadMapInstanceType(right_map);

      GotoIf(Int32LessThan(left_type, Int32Constant(FIRST_NONSTRING_TYPE)),
             &if_left_string);
      GotoIf(InstanceTypeEqual(left_type, SYMBOL_TYPE), &if_left_symbol);
      GotoIf(InstanceTypeEqual(left_type, HEAP_NUMBER_TYPE), &if_left_number);
      GotoIf(InstanceTypeEqual(left_type, ODDBALL_TYPE), &if_left_oddball);
      GotoIf(InstanceTypeEqual(left_type, BIGINT_TYPE), &if_left_bigint);
      Goto(&if_left_receiver);

      BIND(&if_left_string);
      {
        GotoIfNot(IsStringInstanceType(right_type), &use_symmetry);
        result.Bind(CallBuiltin(Builtins::kStringEqual, context, left, right));
        if (var_type_feedback != nullptr) {
          CombineFeedback(var_type_feedback,
                          SmiOr(CollectFeedbackForString(left_type),
                                CollectFeedbackForString(right_type)));
        }
        Goto(&end);
      }

      BIND(&if_left_number);
      {
        Label if_right_not_number(this);
        GotoIf(Word32NotEqual(left_type, right_type), &if_right_not_number);

        var_left_float = LoadHeapNumberValue(left);
        var_right_float = LoadHeapNumberValue(right);
        if (var_type_feedback != nullptr) {
          CombineFeedback(var_type_feedback,
                          SmiConstant(CompareOperationFeedback::kNumber));
        }
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
            var_right.Bind(LoadObjectField(right, Oddball::kToNumberOffset));
            Goto(&loop);
          }
        }
      }

      BIND(&if_left_bigint);
      {
        if (var_type_feedback != nullptr) {
          var_type_feedback->Bind(SmiConstant(CompareOperationFeedback::kAny));
        }

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
          result.Bind(CallRuntime(Runtime::kBigIntEqualToNumber,
                                  NoContextConstant(), left, right));
          Goto(&end);
        }

        BIND(&if_right_bigint);
        {
          result.Bind(CallRuntime(Runtime::kBigIntEqualToBigInt,
                                  NoContextConstant(), left, right));
          Goto(&end);
        }

        BIND(&if_right_string);
        {
          result.Bind(CallRuntime(Runtime::kBigIntEqualToString,
                                  NoContextConstant(), left, right));
          Goto(&end);
        }

        BIND(&if_right_boolean);
        {
          var_right.Bind(LoadObjectField(right, Oddball::kToNumberOffset));
          Goto(&loop);
        }
      }

      BIND(&if_left_oddball);
      {
        if (var_type_feedback != nullptr) {
          var_type_feedback->Bind(SmiConstant(CompareOperationFeedback::kAny));
        }

        Label if_left_boolean(this);
        GotoIf(IsBooleanMap(left_map), &if_left_boolean);
        // {left} is either Null or Undefined. Check if {right} is
        // undetectable (which includes Null and Undefined).
        Branch(IsUndetectableMap(right_map), &if_equal, &if_notequal);

        BIND(&if_left_boolean);
        {
          // If {right} is a Boolean too, it must be a different Boolean.
          GotoIf(WordEqual(right_map, left_map), &if_notequal);
          // Otherwise, convert {left} to number and try again.
          var_left.Bind(LoadObjectField(left, Oddball::kToNumberOffset));
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
                            SmiConstant(CompareOperationFeedback::kSymbol));
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
        Label if_right_not_receiver(this);
        GotoIfNot(IsJSReceiverInstanceType(right_type), &if_right_not_receiver);

        // {left} and {right} are different JSReceiver references.
        if (var_type_feedback != nullptr) {
          CombineFeedback(var_type_feedback,
                          SmiConstant(CompareOperationFeedback::kReceiver));
        }
        Goto(&if_notequal);

        BIND(&if_right_not_receiver);
        {
          if (var_type_feedback != nullptr) {
            var_type_feedback->Bind(
                SmiConstant(CompareOperationFeedback::kAny));
          }
          Label if_right_null_or_undefined(this);
          GotoIf(IsUndetectableMap(right_map), &if_right_null_or_undefined);

          // {right} is a Primitive; convert {left} to Primitive too.
          Callable callable = CodeFactory::NonPrimitiveToPrimitive(isolate());
          var_left.Bind(CallStub(callable, context, left));
          Goto(&loop);

          BIND(&if_right_null_or_undefined);
          Branch(IsUndetectableMap(left_map), &if_equal, &if_notequal);
        }
      }
    }

    BIND(&do_right_stringtonumber);
    {
      var_right.Bind(CallBuiltin(Builtins::kStringToNumber, context, right));
      Goto(&loop);
    }

    BIND(&use_symmetry);
    {
      var_left.Bind(right);
      var_right.Bind(left);
      Goto(&loop);
    }
  }

  BIND(&do_float_comparison);
  {
    Branch(Float64Equal(var_left_float, var_right_float), &if_equal,
           &if_notequal);
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

Node* CodeStubAssembler::StrictEqual(Node* lhs, Node* rhs,
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

  Label if_equal(this), if_notequal(this), end(this);
  VARIABLE(result, MachineRepresentation::kTagged);

  // Check if {lhs} and {rhs} refer to the same object.
  Label if_same(this), if_notsame(this);
  Branch(WordEqual(lhs, rhs), &if_same, &if_notsame);

  BIND(&if_same);
  {
    // The {lhs} and {rhs} reference the exact same value, yet we need special
    // treatment for HeapNumber, as NaN is not equal to NaN.
    if (var_type_feedback != nullptr) {
      var_type_feedback->Bind(SmiConstant(CompareOperationFeedback::kNone));
    }
    GenerateEqual_Same(lhs, &if_equal, &if_notequal, var_type_feedback);
  }

  BIND(&if_notsame);
  {
    // The {lhs} and {rhs} reference different objects, yet for Smi, HeapNumber,
    // BigInt and String they can still be considered equal.

    if (var_type_feedback != nullptr) {
      var_type_feedback->Bind(SmiConstant(CompareOperationFeedback::kAny));
    }

    // Check if {lhs} is a Smi or a HeapObject.
    Label if_lhsissmi(this), if_lhsisnotsmi(this);
    Branch(TaggedIsSmi(lhs), &if_lhsissmi, &if_lhsisnotsmi);

    BIND(&if_lhsisnotsmi);
    {
      // Load the map of {lhs}.
      Node* lhs_map = LoadMap(lhs);

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
          Node* lhs_value = LoadHeapNumberValue(lhs);
          Node* rhs_value = SmiToFloat64(rhs);

          if (var_type_feedback != nullptr) {
            var_type_feedback->Bind(
                SmiConstant(CompareOperationFeedback::kNumber));
          }

          // Perform a floating point comparison of {lhs} and {rhs}.
          Branch(Float64Equal(lhs_value, rhs_value), &if_equal, &if_notequal);
        }

        BIND(&if_rhsisnotsmi);
        {
          // Load the map of {rhs}.
          Node* rhs_map = LoadMap(rhs);

          // Check if {rhs} is also a HeapNumber.
          Label if_rhsisnumber(this), if_rhsisnotnumber(this);
          Branch(IsHeapNumberMap(rhs_map), &if_rhsisnumber, &if_rhsisnotnumber);

          BIND(&if_rhsisnumber);
          {
            // Convert {lhs} and {rhs} to floating point values.
            Node* lhs_value = LoadHeapNumberValue(lhs);
            Node* rhs_value = LoadHeapNumberValue(rhs);

            if (var_type_feedback != nullptr) {
              var_type_feedback->Bind(
                  SmiConstant(CompareOperationFeedback::kNumber));
            }

            // Perform a floating point comparison of {lhs} and {rhs}.
            Branch(Float64Equal(lhs_value, rhs_value), &if_equal, &if_notequal);
          }

          BIND(&if_rhsisnotnumber);
          Goto(&if_notequal);
        }
      }

      BIND(&if_lhsisnotnumber);
      {
        // Check if {rhs} is a Smi or a HeapObject.
        Label if_rhsissmi(this), if_rhsisnotsmi(this);
        Branch(TaggedIsSmi(rhs), &if_rhsissmi, &if_rhsisnotsmi);

        BIND(&if_rhsissmi);
        Goto(&if_notequal);

        BIND(&if_rhsisnotsmi);
        {
          // Load the instance type of {lhs}.
          Node* lhs_instance_type = LoadMapInstanceType(lhs_map);

          // Check if {lhs} is a String.
          Label if_lhsisstring(this), if_lhsisnotstring(this);
          Branch(IsStringInstanceType(lhs_instance_type), &if_lhsisstring,
                 &if_lhsisnotstring);

          BIND(&if_lhsisstring);
          {
            // Load the instance type of {rhs}.
            Node* rhs_instance_type = LoadInstanceType(rhs);

            // Check if {rhs} is also a String.
            Label if_rhsisstring(this, Label::kDeferred),
                if_rhsisnotstring(this);
            Branch(IsStringInstanceType(rhs_instance_type), &if_rhsisstring,
                   &if_rhsisnotstring);

            BIND(&if_rhsisstring);
            {
              if (var_type_feedback != nullptr) {
                Node* lhs_feedback =
                    CollectFeedbackForString(lhs_instance_type);
                Node* rhs_feedback =
                    CollectFeedbackForString(rhs_instance_type);
                var_type_feedback->Bind(SmiOr(lhs_feedback, rhs_feedback));
              }
              result.Bind(CallBuiltin(Builtins::kStringEqual,
                                      NoContextConstant(), lhs, rhs));
              Goto(&end);
            }

            BIND(&if_rhsisnotstring);
            Goto(&if_notequal);
          }

          BIND(&if_lhsisnotstring);

          // Check if {lhs} is a BigInt.
          Label if_lhsisbigint(this), if_lhsisnotbigint(this);
          Branch(IsBigIntInstanceType(lhs_instance_type), &if_lhsisbigint,
                 &if_lhsisnotbigint);

          BIND(&if_lhsisbigint);
          {
            // Load the instance type of {rhs}.
            Node* rhs_instance_type = LoadInstanceType(rhs);

            // Check if {rhs} is also a BigInt.
            Label if_rhsisbigint(this, Label::kDeferred),
                if_rhsisnotbigint(this);
            Branch(IsBigIntInstanceType(rhs_instance_type), &if_rhsisbigint,
                   &if_rhsisnotbigint);

            BIND(&if_rhsisbigint);
            {
              if (var_type_feedback != nullptr) {
                CSA_ASSERT(
                    this,
                    WordEqual(var_type_feedback->value(),
                              SmiConstant(CompareOperationFeedback::kAny)));
              }
              result.Bind(CallRuntime(Runtime::kBigIntEqualToBigInt,
                                      NoContextConstant(), lhs, rhs));
              Goto(&end);
            }

            BIND(&if_rhsisnotbigint);
            Goto(&if_notequal);
          }

          BIND(&if_lhsisnotbigint);
          if (var_type_feedback != nullptr) {
            // Load the instance type of {rhs}.
            Node* rhs_instance_type = LoadInstanceType(rhs);

            Label if_lhsissymbol(this), if_lhsisreceiver(this);
            GotoIf(IsJSReceiverInstanceType(lhs_instance_type),
                   &if_lhsisreceiver);
            Branch(IsSymbolInstanceType(lhs_instance_type), &if_lhsissymbol,
                   &if_notequal);

            BIND(&if_lhsisreceiver);
            {
              GotoIfNot(IsJSReceiverInstanceType(rhs_instance_type),
                        &if_notequal);
              var_type_feedback->Bind(
                  SmiConstant(CompareOperationFeedback::kReceiver));
              Goto(&if_notequal);
            }

            BIND(&if_lhsissymbol);
            {
              GotoIfNot(IsSymbolInstanceType(rhs_instance_type), &if_notequal);
              var_type_feedback->Bind(
                  SmiConstant(CompareOperationFeedback::kSymbol));
              Goto(&if_notequal);
            }
          } else {
            Goto(&if_notequal);
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
      if (var_type_feedback != nullptr) {
        var_type_feedback->Bind(
            SmiConstant(CompareOperationFeedback::kSignedSmall));
      }
      Goto(&if_notequal);

      BIND(&if_rhsisnotsmi);
      {
        // Load the map of the {rhs}.
        Node* rhs_map = LoadMap(rhs);

        // The {rhs} could be a HeapNumber with the same value as {lhs}.
        Label if_rhsisnumber(this), if_rhsisnotnumber(this);
        Branch(IsHeapNumberMap(rhs_map), &if_rhsisnumber, &if_rhsisnotnumber);

        BIND(&if_rhsisnumber);
        {
          // Convert {lhs} and {rhs} to floating point values.
          Node* lhs_value = SmiToFloat64(lhs);
          Node* rhs_value = LoadHeapNumberValue(rhs);

          if (var_type_feedback != nullptr) {
            var_type_feedback->Bind(
                SmiConstant(CompareOperationFeedback::kNumber));
          }

          // Perform a floating point comparison of {lhs} and {rhs}.
          Branch(Float64Equal(lhs_value, rhs_value), &if_equal, &if_notequal);
        }

        BIND(&if_rhsisnotnumber);
        Goto(&if_notequal);
      }
    }
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

// ECMA#sec-samevalue
// This algorithm differs from the Strict Equality Comparison Algorithm in its
// treatment of signed zeroes and NaNs.
void CodeStubAssembler::BranchIfSameValue(Node* lhs, Node* rhs, Label* if_true,
                                          Label* if_false) {
  VARIABLE(var_lhs_value, MachineRepresentation::kFloat64);
  VARIABLE(var_rhs_value, MachineRepresentation::kFloat64);
  Label do_fcmp(this);

  // Immediately jump to {if_true} if {lhs} == {rhs}, because - unlike
  // StrictEqual - SameValue considers two NaNs to be equal.
  GotoIf(WordEqual(lhs, rhs), if_true);

  // Check if the {lhs} is a Smi.
  Label if_lhsissmi(this), if_lhsisheapobject(this);
  Branch(TaggedIsSmi(lhs), &if_lhsissmi, &if_lhsisheapobject);

  BIND(&if_lhsissmi);
  {
    // Since {lhs} is a Smi, the comparison can only yield true
    // iff the {rhs} is a HeapNumber with the same float64 value.
    GotoIf(TaggedIsSmi(rhs), if_false);
    GotoIfNot(IsHeapNumber(rhs), if_false);
    var_lhs_value.Bind(SmiToFloat64(lhs));
    var_rhs_value.Bind(LoadHeapNumberValue(rhs));
    Goto(&do_fcmp);
  }

  BIND(&if_lhsisheapobject);
  {
    // Check if the {rhs} is a Smi.
    Label if_rhsissmi(this), if_rhsisheapobject(this);
    Branch(TaggedIsSmi(rhs), &if_rhsissmi, &if_rhsisheapobject);

    BIND(&if_rhsissmi);
    {
      // Since {rhs} is a Smi, the comparison can only yield true
      // iff the {lhs} is a HeapNumber with the same float64 value.
      GotoIfNot(IsHeapNumber(lhs), if_false);
      var_lhs_value.Bind(LoadHeapNumberValue(lhs));
      var_rhs_value.Bind(SmiToFloat64(rhs));
      Goto(&do_fcmp);
    }

    BIND(&if_rhsisheapobject);
    {
      // Now this can only yield true if either both {lhs} and {rhs} are
      // HeapNumbers with the same value, or both are Strings with the same
      // character sequence, or both are BigInts with the same value.
      Label if_lhsisheapnumber(this), if_lhsisstring(this),
          if_lhsisbigint(this);
      Node* const lhs_map = LoadMap(lhs);
      GotoIf(IsHeapNumberMap(lhs_map), &if_lhsisheapnumber);
      Node* const lhs_instance_type = LoadMapInstanceType(lhs_map);
      GotoIf(IsStringInstanceType(lhs_instance_type), &if_lhsisstring);
      Branch(IsBigIntInstanceType(lhs_instance_type), &if_lhsisbigint,
             if_false);

      BIND(&if_lhsisheapnumber);
      {
        GotoIfNot(IsHeapNumber(rhs), if_false);
        var_lhs_value.Bind(LoadHeapNumberValue(lhs));
        var_rhs_value.Bind(LoadHeapNumberValue(rhs));
        Goto(&do_fcmp);
      }

      BIND(&if_lhsisstring);
      {
        // Now we can only yield true if {rhs} is also a String
        // with the same sequence of characters.
        GotoIfNot(IsString(rhs), if_false);
        Node* const result =
            CallBuiltin(Builtins::kStringEqual, NoContextConstant(), lhs, rhs);
        Branch(IsTrue(result), if_true, if_false);
      }

      BIND(&if_lhsisbigint);
      {
        GotoIfNot(IsBigInt(rhs), if_false);
        Node* const result = CallRuntime(Runtime::kBigIntEqualToBigInt,
                                         NoContextConstant(), lhs, rhs);
        Branch(IsTrue(result), if_true, if_false);
      }
    }
  }

  BIND(&do_fcmp);
  {
    Node* const lhs_value = var_lhs_value.value();
    Node* const rhs_value = var_rhs_value.value();

    Label if_equal(this), if_notequal(this);
    Branch(Float64Equal(lhs_value, rhs_value), &if_equal, &if_notequal);

    BIND(&if_equal);
    {
      // We still need to handle the case when {lhs} and {rhs} are -0.0 and
      // 0.0 (or vice versa). Compare the high word to
      // distinguish between the two.
      Node* const lhs_hi_word = Float64ExtractHighWord32(lhs_value);
      Node* const rhs_hi_word = Float64ExtractHighWord32(rhs_value);

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
}

Node* CodeStubAssembler::HasProperty(Node* object, Node* key, Node* context,
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

  TryPrototypeChainLookup(object, key, lookup_property_in_holder,
                          lookup_element_in_holder, &return_false,
                          &call_runtime, &if_proxy);

  VARIABLE(result, MachineRepresentation::kTagged);

  BIND(&if_proxy);
  {
    Node* name = ToName(context, key);
    switch (mode) {
      case kHasProperty:
        GotoIf(IsPrivateSymbol(name), &return_false);

        result.Bind(
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
    result.Bind(TrueConstant());
    Goto(&end);
  }

  BIND(&return_false);
  {
    result.Bind(FalseConstant());
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

    result.Bind(
        CallRuntime(fallback_runtime_function_id, context, object, key));
    Goto(&end);
  }

  BIND(&end);
  return result.value();
}

Node* CodeStubAssembler::ClassOf(Node* value) {
  VARIABLE(var_result, MachineRepresentation::kTaggedPointer);
  Label if_function_template_info(this, Label::kDeferred),
      if_no_class_name(this, Label::kDeferred),
      if_function(this, Label::kDeferred), if_object(this, Label::kDeferred),
      if_primitive(this, Label::kDeferred), return_result(this);

  // Check if {value} is a Smi.
  GotoIf(TaggedIsSmi(value), &if_primitive);

  Node* value_map = LoadMap(value);
  Node* value_instance_type = LoadMapInstanceType(value_map);

  // Check if {value} is a JSFunction or JSBoundFunction.
  STATIC_ASSERT(LAST_TYPE == LAST_FUNCTION_TYPE);
  GotoIf(Uint32LessThanOrEqual(Int32Constant(FIRST_FUNCTION_TYPE),
                               value_instance_type),
         &if_function);

  // Check if {value} is a primitive HeapObject.
  STATIC_ASSERT(LAST_TYPE == LAST_JS_RECEIVER_TYPE);
  GotoIfNot(IsJSReceiverInstanceType(value_instance_type), &if_primitive);

  // Load the {value}s constructor, and check that it's a JSFunction.
  Node* constructor = LoadMapConstructor(value_map);
  GotoIf(HasInstanceType(constructor, FUNCTION_TEMPLATE_INFO_TYPE),
         &if_function_template_info);
  GotoIfNot(IsJSFunction(constructor), &if_object);

  // Return the instance class name for the {constructor}.
  Node* shared_info =
      LoadObjectField(constructor, JSFunction::kSharedFunctionInfoOffset);
  Node* instance_class_name = LoadObjectField(
      shared_info, SharedFunctionInfo::kInstanceClassNameOffset);
  var_result.Bind(instance_class_name);
  Goto(&return_result);

  // For remote objects the constructor might be given as FTI.
  BIND(&if_function_template_info);
  Node* class_name =
      LoadObjectField(constructor, FunctionTemplateInfo::kClassNameOffset);
  GotoIf(IsUndefined(class_name), &if_no_class_name);
  var_result.Bind(class_name);
  Goto(&return_result);

  BIND(&if_no_class_name);
  var_result.Bind(LoadRoot(Heap::kempty_stringRootIndex));
  Goto(&return_result);

  BIND(&if_function);
  var_result.Bind(LoadRoot(Heap::kFunction_stringRootIndex));
  Goto(&return_result);

  BIND(&if_object);
  var_result.Bind(LoadRoot(Heap::kObject_stringRootIndex));
  Goto(&return_result);

  BIND(&if_primitive);
  var_result.Bind(NullConstant());
  Goto(&return_result);

  BIND(&return_result);
  return var_result.value();
}

Node* CodeStubAssembler::Typeof(Node* value) {
  VARIABLE(result_var, MachineRepresentation::kTagged);

  Label return_number(this, Label::kDeferred), if_oddball(this),
      return_function(this), return_undefined(this), return_object(this),
      return_string(this), return_bigint(this), return_result(this);

  GotoIf(TaggedIsSmi(value), &return_number);

  Node* map = LoadMap(value);

  GotoIf(IsHeapNumberMap(map), &return_number);

  Node* instance_type = LoadMapInstanceType(map);

  GotoIf(InstanceTypeEqual(instance_type, ODDBALL_TYPE), &if_oddball);

  Node* callable_or_undetectable_mask = Word32And(
      LoadMapBitField(map),
      Int32Constant(1 << Map::kIsCallable | 1 << Map::kIsUndetectable));

  GotoIf(Word32Equal(callable_or_undetectable_mask,
                     Int32Constant(1 << Map::kIsCallable)),
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
    Node* type = LoadObjectField(value, Oddball::kTypeOfOffset);
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

Node* CodeStubAssembler::GetSuperConstructor(Node* active_function,
                                             Node* context) {
  CSA_ASSERT(this, IsJSFunction(active_function));

  Label is_not_constructor(this, Label::kDeferred), out(this);
  VARIABLE(result, MachineRepresentation::kTagged);

  Node* map = LoadMap(active_function);
  Node* prototype = LoadMapPrototype(map);
  Node* prototype_map = LoadMap(prototype);
  GotoIfNot(IsConstructorMap(prototype_map), &is_not_constructor);

  result.Bind(prototype);
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
  Node* inst_of_handler =
      GetProperty(context, callable, HasInstanceSymbolConstant());

  // Optimize for the likely case where {inst_of_handler} is the builtin
  // Function.prototype[@@hasInstance] method, and emit a direct call in
  // that case without any additional checking.
  Node* native_context = LoadNativeContext(context);
  Node* function_has_instance =
      LoadContextElement(native_context, Context::FUNCTION_HAS_INSTANCE_INDEX);
  GotoIfNot(WordEqual(inst_of_handler, function_has_instance),
            &if_otherhandler);
  {
    // TODO(6786): A direct call to a TFJ builtin breaks the lazy
    // deserialization mechanism in two ways: first, we always pass in a
    // callable containing the DeserializeLazy code object (assuming that
    // FunctionPrototypeHasInstance is lazy). Second, a direct call (without
    // going through CodeFactory::Call) to DeserializeLazy will not initialize
    // new_target properly. For now we can avoid this by marking
    // FunctionPrototypeHasInstance as eager, but this should be fixed at some
    // point.
    //
    // Call to Function.prototype[@@hasInstance] directly.
    Callable builtin(BUILTIN_CODE(isolate(), FunctionPrototypeHasInstance),
                     CallTrampolineDescriptor(isolate()));
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
    Node* result =
        CallBuiltin(Builtins::kOrdinaryHasInstance, context, callable, object);
    var_result.Bind(result);
    Goto(&return_result);
  }

  BIND(&if_notcallable);
  {
    CallRuntime(Runtime::kThrowNonCallableInInstanceOfCheck, context);
    Unreachable();
  }

  BIND(&if_notreceiver);
  {
    CallRuntime(Runtime::kThrowNonObjectInInstanceOfCheck, context);
    Unreachable();
  }

  BIND(&return_true);
  var_result.Bind(TrueConstant());
  Goto(&return_result);

  BIND(&return_false);
  var_result.Bind(FalseConstant());
  Goto(&return_result);

  BIND(&return_result);
  return var_result.value();
}

Node* CodeStubAssembler::NumberInc(Node* value) {
  VARIABLE(var_result, MachineRepresentation::kTagged);
  VARIABLE(var_finc_value, MachineRepresentation::kFloat64);
  Label if_issmi(this), if_isnotsmi(this), do_finc(this), end(this);
  Branch(TaggedIsSmi(value), &if_issmi, &if_isnotsmi);

  BIND(&if_issmi);
  {
    // Try fast Smi addition first.
    Node* one = SmiConstant(1);
    Node* pair = IntPtrAddWithOverflow(BitcastTaggedToWord(value),
                                       BitcastTaggedToWord(one));
    Node* overflow = Projection(1, pair);

    // Check if the Smi addition overflowed.
    Label if_overflow(this), if_notoverflow(this);
    Branch(overflow, &if_overflow, &if_notoverflow);

    BIND(&if_notoverflow);
    var_result.Bind(BitcastWordToTaggedSigned(Projection(0, pair)));
    Goto(&end);

    BIND(&if_overflow);
    {
      var_finc_value.Bind(SmiToFloat64(value));
      Goto(&do_finc);
    }
  }

  BIND(&if_isnotsmi);
  {
    CSA_ASSERT(this, IsHeapNumber(value));

    // Load the HeapNumber value.
    var_finc_value.Bind(LoadHeapNumberValue(value));
    Goto(&do_finc);
  }

  BIND(&do_finc);
  {
    Node* finc_value = var_finc_value.value();
    Node* one = Float64Constant(1.0);
    Node* finc_result = Float64Add(finc_value, one);
    var_result.Bind(AllocateHeapNumberWithValue(finc_result));
    Goto(&end);
  }

  BIND(&end);
  return var_result.value();
}

Node* CodeStubAssembler::NumberDec(Node* value) {
  VARIABLE(var_result, MachineRepresentation::kTagged);
  VARIABLE(var_fdec_value, MachineRepresentation::kFloat64);
  Label if_issmi(this), if_isnotsmi(this), do_fdec(this), end(this);
  Branch(TaggedIsSmi(value), &if_issmi, &if_isnotsmi);

  BIND(&if_issmi);
  {
    // Try fast Smi addition first.
    Node* one = SmiConstant(1);
    Node* pair = IntPtrSubWithOverflow(BitcastTaggedToWord(value),
                                       BitcastTaggedToWord(one));
    Node* overflow = Projection(1, pair);

    // Check if the Smi addition overflowed.
    Label if_overflow(this), if_notoverflow(this);
    Branch(overflow, &if_overflow, &if_notoverflow);

    BIND(&if_notoverflow);
    var_result.Bind(BitcastWordToTaggedSigned(Projection(0, pair)));
    Goto(&end);

    BIND(&if_overflow);
    {
      var_fdec_value.Bind(SmiToFloat64(value));
      Goto(&do_fdec);
    }
  }

  BIND(&if_isnotsmi);
  {
    CSA_ASSERT(this, IsHeapNumber(value));

    // Load the HeapNumber value.
    var_fdec_value.Bind(LoadHeapNumberValue(value));
    Goto(&do_fdec);
  }

  BIND(&do_fdec);
  {
    Node* fdec_value = var_fdec_value.value();
    Node* minus_one = Float64Constant(-1.0);
    Node* fdec_result = Float64Add(fdec_value, minus_one);
    var_result.Bind(AllocateHeapNumberWithValue(fdec_result));
    Goto(&end);
  }

  BIND(&end);
  return var_result.value();
}

Node* CodeStubAssembler::NumberAdd(Node* a, Node* b) {
  VARIABLE(var_result, MachineRepresentation::kTagged);
  VARIABLE(var_fadd_value, MachineRepresentation::kFloat64);
  Label float_add(this, Label::kDeferred), end(this);
  GotoIf(TaggedIsNotSmi(a), &float_add);
  GotoIf(TaggedIsNotSmi(b), &float_add);

  // Try fast Smi addition first.
  Node* pair =
      IntPtrAddWithOverflow(BitcastTaggedToWord(a), BitcastTaggedToWord(b));
  Node* overflow = Projection(1, pair);

  // Check if the Smi addition overflowed.
  Label if_overflow(this), if_notoverflow(this);
  GotoIf(overflow, &float_add);

  var_result.Bind(BitcastWordToTaggedSigned(Projection(0, pair)));
  Goto(&end);

  BIND(&float_add);
  {
    var_result.Bind(ChangeFloat64ToTagged(
        Float64Add(ChangeNumberToFloat64(a), ChangeNumberToFloat64(b))));
    Goto(&end);
  }

  BIND(&end);
  return var_result.value();
}

Node* CodeStubAssembler::NumberSub(Node* a, Node* b) {
  VARIABLE(var_result, MachineRepresentation::kTagged);
  VARIABLE(var_fsub_value, MachineRepresentation::kFloat64);
  Label float_sub(this, Label::kDeferred), end(this);
  GotoIf(TaggedIsNotSmi(a), &float_sub);
  GotoIf(TaggedIsNotSmi(b), &float_sub);

  // Try fast Smi subtraction first.
  Node* pair =
      IntPtrSubWithOverflow(BitcastTaggedToWord(a), BitcastTaggedToWord(b));
  Node* overflow = Projection(1, pair);

  // Check if the Smi subtraction overflowed.
  Label if_overflow(this), if_notoverflow(this);
  GotoIf(overflow, &float_sub);

  var_result.Bind(BitcastWordToTaggedSigned(Projection(0, pair)));
  Goto(&end);

  BIND(&float_sub);
  {
    var_result.Bind(ChangeFloat64ToTagged(
        Float64Sub(ChangeNumberToFloat64(a), ChangeNumberToFloat64(b))));
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

Node* CodeStubAssembler::BitwiseOp(Node* left32, Node* right32,
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
        right32 = Word32And(right32, Int32Constant(0x1f));
      }
      return ChangeInt32ToTagged(Signed(Word32Shl(left32, right32)));
    case Operation::kShiftRight:
      if (!Word32ShiftIsSafe()) {
        right32 = Word32And(right32, Int32Constant(0x1f));
      }
      return ChangeInt32ToTagged(Signed(Word32Sar(left32, right32)));
    case Operation::kShiftRightLogical:
      if (!Word32ShiftIsSafe()) {
        right32 = Word32And(right32, Int32Constant(0x1f));
      }
      return ChangeUint32ToTagged(Unsigned(Word32Shr(left32, right32)));
    default:
      break;
  }
  UNREACHABLE();
}

Node* CodeStubAssembler::CreateArrayIterator(Node* array, Node* array_map,
                                             Node* array_type, Node* context,
                                             IterationKind mode) {
  int kBaseMapIndex = 0;
  switch (mode) {
    case IterationKind::kKeys:
      kBaseMapIndex = Context::TYPED_ARRAY_KEY_ITERATOR_MAP_INDEX;
      break;
    case IterationKind::kValues:
      kBaseMapIndex = Context::UINT8_ARRAY_VALUE_ITERATOR_MAP_INDEX;
      break;
    case IterationKind::kEntries:
      kBaseMapIndex = Context::UINT8_ARRAY_KEY_VALUE_ITERATOR_MAP_INDEX;
      break;
  }

  // Fast Array iterator map index:
  // (kBaseIndex + kFastIteratorOffset) + ElementsKind (for JSArrays)
  // kBaseIndex + (ElementsKind - UINT8_ELEMENTS) (for JSTypedArrays)
  const int kFastIteratorOffset =
      Context::FAST_SMI_ARRAY_VALUE_ITERATOR_MAP_INDEX -
      Context::UINT8_ARRAY_VALUE_ITERATOR_MAP_INDEX;
  STATIC_ASSERT(kFastIteratorOffset ==
                (Context::FAST_SMI_ARRAY_KEY_VALUE_ITERATOR_MAP_INDEX -
                 Context::UINT8_ARRAY_KEY_VALUE_ITERATOR_MAP_INDEX));

  // Slow Array iterator map index: (kBaseIndex + kSlowIteratorOffset)
  const int kSlowIteratorOffset =
      Context::GENERIC_ARRAY_VALUE_ITERATOR_MAP_INDEX -
      Context::UINT8_ARRAY_VALUE_ITERATOR_MAP_INDEX;
  STATIC_ASSERT(kSlowIteratorOffset ==
                (Context::GENERIC_ARRAY_KEY_VALUE_ITERATOR_MAP_INDEX -
                 Context::UINT8_ARRAY_KEY_VALUE_ITERATOR_MAP_INDEX));

  // Assert: Type(array) is Object
  CSA_ASSERT(this, IsJSReceiverInstanceType(array_type));

  VARIABLE(var_result, MachineRepresentation::kTagged);
  VARIABLE(var_map_index, MachineType::PointerRepresentation());
  VARIABLE(var_array_map, MachineRepresentation::kTagged);

  Label return_result(this);
  Label allocate_iterator(this);

  if (mode == IterationKind::kKeys) {
    // There are only two key iterator maps, branch depending on whether or not
    // the receiver is a TypedArray or not.

    Label if_istypedarray(this), if_isgeneric(this);

    Branch(InstanceTypeEqual(array_type, JS_TYPED_ARRAY_TYPE), &if_istypedarray,
           &if_isgeneric);

    BIND(&if_isgeneric);
    {
      Label if_isfast(this), if_isslow(this);
      BranchIfFastJSArray(array, context, &if_isfast, &if_isslow);

      BIND(&if_isfast);
      {
        var_map_index.Bind(
            IntPtrConstant(Context::FAST_ARRAY_KEY_ITERATOR_MAP_INDEX));
        var_array_map.Bind(array_map);
        Goto(&allocate_iterator);
      }

      BIND(&if_isslow);
      {
        var_map_index.Bind(
            IntPtrConstant(Context::GENERIC_ARRAY_KEY_ITERATOR_MAP_INDEX));
        var_array_map.Bind(UndefinedConstant());
        Goto(&allocate_iterator);
      }
    }

    BIND(&if_istypedarray);
    {
      var_map_index.Bind(
          IntPtrConstant(Context::TYPED_ARRAY_KEY_ITERATOR_MAP_INDEX));
      var_array_map.Bind(UndefinedConstant());
      Goto(&allocate_iterator);
    }
  } else {
    Label if_istypedarray(this), if_isgeneric(this);
    Branch(InstanceTypeEqual(array_type, JS_TYPED_ARRAY_TYPE), &if_istypedarray,
           &if_isgeneric);

    BIND(&if_isgeneric);
    {
      Label if_isfast(this), if_isslow(this);
      BranchIfFastJSArray(array, context, &if_isfast, &if_isslow);

      BIND(&if_isfast);
      {
        Label if_ispacked(this), if_isholey(this);
        Node* elements_kind = LoadMapElementsKind(array_map);
        Branch(IsHoleyFastElementsKind(elements_kind), &if_isholey,
               &if_ispacked);

        BIND(&if_isholey);
        {
          // Fast holey JSArrays can treat the hole as undefined if the
          // protector cell is valid, and the prototype chain is unchanged from
          // its initial state (because the protector cell is only tracked for
          // initial the Array and Object prototypes). Check these conditions
          // here, and take the slow path if any fail.
          GotoIf(IsNoElementsProtectorCellInvalid(), &if_isslow);

          Node* native_context = LoadNativeContext(context);

          Node* prototype = LoadMapPrototype(array_map);
          Node* array_prototype = LoadContextElement(
              native_context, Context::INITIAL_ARRAY_PROTOTYPE_INDEX);
          GotoIfNot(WordEqual(prototype, array_prototype), &if_isslow);

          Node* map = LoadMap(prototype);
          prototype = LoadMapPrototype(map);
          Node* object_prototype = LoadContextElement(
              native_context, Context::INITIAL_OBJECT_PROTOTYPE_INDEX);
          GotoIfNot(WordEqual(prototype, object_prototype), &if_isslow);

          map = LoadMap(prototype);
          prototype = LoadMapPrototype(map);
          Branch(IsNull(prototype), &if_ispacked, &if_isslow);
        }
        BIND(&if_ispacked);
        {
          Node* map_index =
              IntPtrAdd(IntPtrConstant(kBaseMapIndex + kFastIteratorOffset),
                        ChangeUint32ToWord(LoadMapElementsKind(array_map)));
          CSA_ASSERT(this, IntPtrGreaterThanOrEqual(
                               map_index, IntPtrConstant(kBaseMapIndex +
                                                         kFastIteratorOffset)));
          CSA_ASSERT(this, IntPtrLessThan(map_index,
                                          IntPtrConstant(kBaseMapIndex +
                                                         kSlowIteratorOffset)));

          var_map_index.Bind(map_index);
          var_array_map.Bind(array_map);
          Goto(&allocate_iterator);
        }
      }

      BIND(&if_isslow);
      {
        Node* map_index = IntPtrAdd(IntPtrConstant(kBaseMapIndex),
                                    IntPtrConstant(kSlowIteratorOffset));
        var_map_index.Bind(map_index);
        var_array_map.Bind(UndefinedConstant());
        Goto(&allocate_iterator);
      }
    }

    BIND(&if_istypedarray);
    {
      Node* map_index =
          IntPtrAdd(IntPtrConstant(kBaseMapIndex - UINT8_ELEMENTS),
                    ChangeUint32ToWord(LoadMapElementsKind(array_map)));
      CSA_ASSERT(
          this, IntPtrLessThan(map_index, IntPtrConstant(kBaseMapIndex +
                                                         kFastIteratorOffset)));
      CSA_ASSERT(this, IntPtrGreaterThanOrEqual(map_index,
                                                IntPtrConstant(kBaseMapIndex)));
      var_map_index.Bind(map_index);
      var_array_map.Bind(UndefinedConstant());
      Goto(&allocate_iterator);
    }
  }

  BIND(&allocate_iterator);
  {
    Node* map = LoadFixedArrayElement(LoadNativeContext(context),
                                      var_map_index.value());
    var_result.Bind(AllocateJSArrayIterator(array, var_array_map.value(), map));
    Goto(&return_result);
  }

  BIND(&return_result);
  return var_result.value();
}

Node* CodeStubAssembler::AllocateJSArrayIterator(Node* array, Node* array_map,
                                                 Node* map) {
  Node* iterator = Allocate(JSArrayIterator::kSize);
  StoreMapNoWriteBarrier(iterator, map);
  StoreObjectFieldRoot(iterator, JSArrayIterator::kPropertiesOrHashOffset,
                       Heap::kEmptyFixedArrayRootIndex);
  StoreObjectFieldRoot(iterator, JSArrayIterator::kElementsOffset,
                       Heap::kEmptyFixedArrayRootIndex);
  StoreObjectFieldNoWriteBarrier(iterator,
                                 JSArrayIterator::kIteratedObjectOffset, array);
  StoreObjectFieldNoWriteBarrier(iterator, JSArrayIterator::kNextIndexOffset,
                                 SmiConstant(0));
  StoreObjectFieldNoWriteBarrier(
      iterator, JSArrayIterator::kIteratedObjectMapOffset, array_map);
  return iterator;
}

Node* CodeStubAssembler::AllocateJSIteratorResult(Node* context, Node* value,
                                                  Node* done) {
  CSA_ASSERT(this, IsBoolean(done));
  Node* native_context = LoadNativeContext(context);
  Node* map =
      LoadContextElement(native_context, Context::ITERATOR_RESULT_MAP_INDEX);
  Node* result = Allocate(JSIteratorResult::kSize);
  StoreMapNoWriteBarrier(result, map);
  StoreObjectFieldRoot(result, JSIteratorResult::kPropertiesOrHashOffset,
                       Heap::kEmptyFixedArrayRootIndex);
  StoreObjectFieldRoot(result, JSIteratorResult::kElementsOffset,
                       Heap::kEmptyFixedArrayRootIndex);
  StoreObjectFieldNoWriteBarrier(result, JSIteratorResult::kValueOffset, value);
  StoreObjectFieldNoWriteBarrier(result, JSIteratorResult::kDoneOffset, done);
  return result;
}

Node* CodeStubAssembler::AllocateJSIteratorResultForEntry(Node* context,
                                                          Node* key,
                                                          Node* value) {
  Node* native_context = LoadNativeContext(context);
  Node* length = SmiConstant(2);
  int const elements_size = FixedArray::SizeFor(2);
  Node* elements =
      Allocate(elements_size + JSArray::kSize + JSIteratorResult::kSize);
  StoreObjectFieldRoot(elements, FixedArray::kMapOffset,
                       Heap::kFixedArrayMapRootIndex);
  StoreObjectFieldNoWriteBarrier(elements, FixedArray::kLengthOffset, length);
  StoreFixedArrayElement(elements, 0, key);
  StoreFixedArrayElement(elements, 1, value);
  Node* array_map = LoadContextElement(
      native_context, Context::JS_ARRAY_PACKED_ELEMENTS_MAP_INDEX);
  Node* array = InnerAllocate(elements, elements_size);
  StoreMapNoWriteBarrier(array, array_map);
  StoreObjectFieldRoot(array, JSArray::kPropertiesOrHashOffset,
                       Heap::kEmptyFixedArrayRootIndex);
  StoreObjectFieldNoWriteBarrier(array, JSArray::kElementsOffset, elements);
  StoreObjectFieldNoWriteBarrier(array, JSArray::kLengthOffset, length);
  Node* iterator_map =
      LoadContextElement(native_context, Context::ITERATOR_RESULT_MAP_INDEX);
  Node* result = InnerAllocate(array, JSArray::kSize);
  StoreMapNoWriteBarrier(result, iterator_map);
  StoreObjectFieldRoot(result, JSIteratorResult::kPropertiesOrHashOffset,
                       Heap::kEmptyFixedArrayRootIndex);
  StoreObjectFieldRoot(result, JSIteratorResult::kElementsOffset,
                       Heap::kEmptyFixedArrayRootIndex);
  StoreObjectFieldNoWriteBarrier(result, JSIteratorResult::kValueOffset, array);
  StoreObjectFieldRoot(result, JSIteratorResult::kDoneOffset,
                       Heap::kFalseValueRootIndex);
  return result;
}

Node* CodeStubAssembler::TypedArraySpeciesCreateByLength(Node* context,
                                                         Node* originalArray,
                                                         Node* len) {
  // TODO(tebbi): Install a fast path as well, which avoids the runtime
  // call.
  return CallRuntime(Runtime::kTypedArraySpeciesCreateByLength, context,
                     originalArray, len);
}

Node* CodeStubAssembler::IsDetachedBuffer(Node* buffer) {
  CSA_ASSERT(this, HasInstanceType(buffer, JS_ARRAY_BUFFER_TYPE));

  Node* buffer_bit_field = LoadObjectField(
      buffer, JSArrayBuffer::kBitFieldOffset, MachineType::Uint32());
  return IsSetWord32<JSArrayBuffer::WasNeutered>(buffer_bit_field);
}

CodeStubArguments::CodeStubArguments(
    CodeStubAssembler* assembler, SloppyTNode<IntPtrT> argc, Node* fp,
    CodeStubAssembler::ParameterMode param_mode, ReceiverMode receiver_mode)
    : assembler_(assembler),
      argc_mode_(param_mode),
      receiver_mode_(receiver_mode),
      argc_(argc),
      arguments_(),
      fp_(fp != nullptr ? fp : assembler_->LoadFramePointer()) {
  Node* offset = assembler_->ElementOffsetFromIndex(
      argc_, PACKED_ELEMENTS, param_mode,
      (StandardFrameConstants::kFixedSlotCountAboveFp - 1) * kPointerSize);
  arguments_ = assembler_->UncheckedCast<RawPtr<Object>>(
      assembler_->IntPtrAdd(fp_, offset));
}

TNode<Object> CodeStubArguments::GetReceiver() const {
  DCHECK_EQ(receiver_mode_, ReceiverMode::kHasReceiver);
  return assembler_->UncheckedCast<Object>(
      assembler_->Load(MachineType::AnyTagged(), arguments_,
                       assembler_->IntPtrConstant(kPointerSize)));
}

TNode<RawPtr<Object>> CodeStubArguments::AtIndexPtr(
    Node* index, CodeStubAssembler::ParameterMode mode) const {
  typedef compiler::Node Node;
  Node* negated_index = assembler_->IntPtrOrSmiSub(
      assembler_->IntPtrOrSmiConstant(0, mode), index, mode);
  Node* offset = assembler_->ElementOffsetFromIndex(negated_index,
                                                    PACKED_ELEMENTS, mode, 0);
  return assembler_->UncheckedCast<RawPtr<Object>>(assembler_->IntPtrAdd(
      assembler_->UncheckedCast<IntPtrT>(arguments_), offset));
}

TNode<Object> CodeStubArguments::AtIndex(
    Node* index, CodeStubAssembler::ParameterMode mode) const {
  DCHECK_EQ(argc_mode_, mode);
  CSA_ASSERT(assembler_,
             assembler_->UintPtrOrSmiLessThan(index, GetLength(), mode));
  return assembler_->UncheckedCast<Object>(
      assembler_->Load(MachineType::AnyTagged(), AtIndexPtr(index, mode)));
}

TNode<Object> CodeStubArguments::AtIndex(int index) const {
  return AtIndex(assembler_->IntPtrConstant(index));
}

TNode<Object> CodeStubArguments::GetOptionalArgumentValue(
    int index, SloppyTNode<Object> default_value) {
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
  return result;
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
  Node* start = assembler_->IntPtrSub(
      assembler_->UncheckedCast<IntPtrT>(arguments_),
      assembler_->ElementOffsetFromIndex(first, PACKED_ELEMENTS, mode));
  Node* end = assembler_->IntPtrSub(
      assembler_->UncheckedCast<IntPtrT>(arguments_),
      assembler_->ElementOffsetFromIndex(last, PACKED_ELEMENTS, mode));
  assembler_->BuildFastLoop(vars, start, end,
                            [this, &body](Node* current) {
                              Node* arg = assembler_->Load(
                                  MachineType::AnyTagged(), current);
                              body(arg);
                            },
                            -kPointerSize, CodeStubAssembler::INTPTR_PARAMETERS,
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
  assembler_->PopAndReturn(pop_count, value);
}

Node* CodeStubAssembler::IsFastElementsKind(Node* elements_kind) {
  return Uint32LessThanOrEqual(elements_kind,
                               Int32Constant(LAST_FAST_ELEMENTS_KIND));
}

Node* CodeStubAssembler::IsFastSmiOrTaggedElementsKind(Node* elements_kind) {
  return Uint32LessThanOrEqual(elements_kind,
                               Int32Constant(TERMINAL_FAST_ELEMENTS_KIND));
}

Node* CodeStubAssembler::IsHoleyFastElementsKind(Node* elements_kind) {
  CSA_ASSERT(this, IsFastElementsKind(elements_kind));

  STATIC_ASSERT(HOLEY_SMI_ELEMENTS == (PACKED_SMI_ELEMENTS | 1));
  STATIC_ASSERT(HOLEY_ELEMENTS == (PACKED_ELEMENTS | 1));
  STATIC_ASSERT(HOLEY_DOUBLE_ELEMENTS == (PACKED_DOUBLE_ELEMENTS | 1));
  return IsSetWord32(elements_kind, 1);
}

Node* CodeStubAssembler::IsElementsKindGreaterThan(
    Node* target_kind, ElementsKind reference_kind) {
  return Int32GreaterThan(target_kind, Int32Constant(reference_kind));
}

Node* CodeStubAssembler::IsDebugActive() {
  Node* is_debug_active = Load(
      MachineType::Uint8(),
      ExternalConstant(ExternalReference::debug_is_active_address(isolate())));
  return Word32NotEqual(is_debug_active, Int32Constant(0));
}

Node* CodeStubAssembler::IsPromiseHookEnabledOrDebugIsActive() {
  Node* const promise_hook_or_debug_is_active =
      Load(MachineType::Uint8(),
           ExternalConstant(
               ExternalReference::promise_hook_or_debug_is_active_address(
                   isolate())));
  return Word32NotEqual(promise_hook_or_debug_is_active, Int32Constant(0));
}

Node* CodeStubAssembler::AllocateFunctionWithMapAndContext(Node* map,
                                                           Node* shared_info,
                                                           Node* context) {
  CSA_SLOW_ASSERT(this, IsMap(map));

  Node* const code =
      LoadObjectField(shared_info, SharedFunctionInfo::kCodeOffset);

  // TODO(ishell): All the callers of this function pass map loaded from
  // Context::STRICT_FUNCTION_WITHOUT_PROTOTYPE_MAP_INDEX. So we can remove
  // map parameter.
  CSA_ASSERT(this, Word32BinaryNot(IsConstructorMap(map)));
  CSA_ASSERT(this, Word32BinaryNot(IsFunctionWithPrototypeSlotMap(map)));
  Node* const fun = Allocate(JSFunction::kSizeWithoutPrototype);
  StoreMapNoWriteBarrier(fun, map);
  StoreObjectFieldRoot(fun, JSObject::kPropertiesOrHashOffset,
                       Heap::kEmptyFixedArrayRootIndex);
  StoreObjectFieldRoot(fun, JSObject::kElementsOffset,
                       Heap::kEmptyFixedArrayRootIndex);
  StoreObjectFieldRoot(fun, JSFunction::kFeedbackVectorOffset,
                       Heap::kUndefinedCellRootIndex);
  StoreObjectFieldNoWriteBarrier(fun, JSFunction::kSharedFunctionInfoOffset,
                                 shared_info);
  StoreObjectFieldNoWriteBarrier(fun, JSFunction::kContextOffset, context);
  StoreObjectFieldNoWriteBarrier(fun, JSFunction::kCodeOffset, code);
  return fun;
}

Node* CodeStubAssembler::AllocatePromiseReactionJobInfo(
    Node* value, Node* tasks, Node* deferred_promise, Node* deferred_on_resolve,
    Node* deferred_on_reject, Node* context) {
  Node* const result = Allocate(PromiseReactionJobInfo::kSize);
  StoreMapNoWriteBarrier(result, Heap::kPromiseReactionJobInfoMapRootIndex);
  StoreObjectFieldNoWriteBarrier(result, PromiseReactionJobInfo::kValueOffset,
                                 value);
  StoreObjectFieldNoWriteBarrier(result, PromiseReactionJobInfo::kTasksOffset,
                                 tasks);
  StoreObjectFieldNoWriteBarrier(
      result, PromiseReactionJobInfo::kDeferredPromiseOffset, deferred_promise);
  StoreObjectFieldNoWriteBarrier(
      result, PromiseReactionJobInfo::kDeferredOnResolveOffset,
      deferred_on_resolve);
  StoreObjectFieldNoWriteBarrier(
      result, PromiseReactionJobInfo::kDeferredOnRejectOffset,
      deferred_on_reject);
  StoreObjectFieldNoWriteBarrier(result, PromiseReactionJobInfo::kContextOffset,
                                 context);
  return result;
}

Node* CodeStubAssembler::MarkerIsFrameType(Node* marker_or_function,
                                           StackFrame::Type frame_type) {
  return WordEqual(marker_or_function,
                   IntPtrConstant(StackFrame::TypeToMarker(frame_type)));
}

Node* CodeStubAssembler::MarkerIsNotFrameType(Node* marker_or_function,
                                              StackFrame::Type frame_type) {
  return WordNotEqual(marker_or_function,
                      IntPtrConstant(StackFrame::TypeToMarker(frame_type)));
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
    STATIC_ASSERT(JSObject::kElementsOffset == JSProxy::kTargetOffset);
    Node* object_elements = LoadObjectField(object, JSObject::kElementsOffset);
    GotoIf(IsEmptyFixedArray(object_elements), &if_no_elements);
    GotoIf(IsEmptySlowElementDictionary(object_elements), &if_no_elements);

    // It might still be an empty JSArray.
    GotoIfNot(IsJSArrayMap(object_map), if_slow);
    Node* object_length = LoadJSArrayLength(object);
    Branch(WordEqual(object_length, SmiConstant(0)), &if_no_elements, if_slow);

    // Continue with the {object}s prototype.
    BIND(&if_no_elements);
    object = LoadMapPrototype(object_map);
    GotoIf(IsNull(object), if_fast);

    // For all {object}s but the {receiver}, check that the cache is empty.
    var_object.Bind(object);
    object_map = LoadMap(object);
    var_object_map.Bind(object_map);
    Node* object_enum_length = LoadMapEnumLength(object_map);
    Branch(WordEqual(object_enum_length, IntPtrConstant(0)), &loop, if_slow);
  }
}

Node* CodeStubAssembler::CheckEnumCache(Node* receiver, Label* if_empty,
                                        Label* if_runtime) {
  Label if_fast(this), if_cache(this), if_no_cache(this, Label::kDeferred);
  Node* receiver_map = LoadMap(receiver);

  // Check if the enum length field of the {receiver} is properly initialized,
  // indicating that there is an enum cache.
  Node* receiver_enum_length = LoadMapEnumLength(receiver_map);
  Branch(WordEqual(receiver_enum_length,
                   IntPtrConstant(kInvalidEnumCacheSentinel)),
         &if_no_cache, &if_cache);

  BIND(&if_no_cache);
  {
    // Avoid runtime-call for empty dictionary receivers.
    GotoIfNot(IsDictionaryMap(receiver_map), if_runtime);
    Node* properties = LoadSlowProperties(receiver);
    Node* length = LoadFixedArrayElement(
        properties, NameDictionary::kNumberOfElementsIndex);
    GotoIfNot(WordEqual(length, SmiConstant(0)), if_runtime);
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
        formatted.c_str(), TENURED);
    CallRuntime(Runtime::kGlobalPrint, NoContextConstant(),
                HeapConstant(string));
  }
  CallRuntime(Runtime::kDebugPrint, NoContextConstant(), tagged_value);
}

void CodeStubAssembler::PerformStackCheck(Node* context) {
  Label ok(this), stack_check_interrupt(this, Label::kDeferred);

  Node* sp = LoadStackPointer();
  Node* stack_limit = Load(
      MachineType::Pointer(),
      ExternalConstant(ExternalReference::address_of_stack_limit(isolate())));
  Node* interrupt = UintPtrLessThan(sp, stack_limit);

  Branch(interrupt, &stack_check_interrupt, &ok);

  BIND(&stack_check_interrupt);
  {
    CallRuntime(Runtime::kStackGuard, context);
    Goto(&ok);
  }

  BIND(&ok);
}

}  // namespace internal
}  // namespace v8
