// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "src/code-stub-assembler.h"
#include "src/code-factory.h"
#include "src/frames-inl.h"
#include "src/frames.h"
#include "src/ic/handler-configuration.h"
#include "src/ic/stub-cache.h"

namespace v8 {
namespace internal {

using compiler::Node;

CodeStubAssembler::CodeStubAssembler(Isolate* isolate, Zone* zone,
                                     const CallInterfaceDescriptor& descriptor,
                                     Code::Flags flags, const char* name,
                                     size_t result_size)
    : compiler::CodeAssembler(isolate, zone, descriptor, flags, name,
                              result_size) {}

CodeStubAssembler::CodeStubAssembler(Isolate* isolate, Zone* zone,
                                     int parameter_count, Code::Flags flags,
                                     const char* name)
    : compiler::CodeAssembler(isolate, zone, parameter_count, flags, name) {}

void CodeStubAssembler::Assert(ConditionBody codition_body, const char* message,
                               const char* file, int line) {
#if defined(DEBUG)
  Label ok(this);
  Label not_ok(this, Label::kDeferred);
  if (message != nullptr && FLAG_code_comments) {
    Comment("[ Assert: %s", message);
  } else {
    Comment("[ Assert");
  }
  Node* condition = codition_body();
  DCHECK_NOT_NULL(condition);
  Branch(condition, &ok, &not_ok);
  Bind(&not_ok);
  if (message != nullptr) {
    char chars[1024];
    Vector<char> buffer(chars);
    if (file != nullptr) {
      SNPrintF(buffer, "CSA_ASSERT failed: %s [%s:%d]\n", message, file, line);
    } else {
      SNPrintF(buffer, "CSA_ASSERT failed: %s\n", message);
    }
    CallRuntime(
        Runtime::kGlobalPrint, SmiConstant(Smi::kZero),
        HeapConstant(factory()->NewStringFromAsciiChecked(&(buffer[0]))));
  }
  DebugBreak();
  Goto(&ok);
  Bind(&ok);
  Comment("] Assert");
#endif
}

Node* CodeStubAssembler::NoContextConstant() { return NumberConstant(0); }

#define HEAP_CONSTANT_ACCESSOR(rootName, name)     \
  Node* CodeStubAssembler::name##Constant() {      \
    return LoadRoot(Heap::k##rootName##RootIndex); \
  }
HEAP_CONSTANT_LIST(HEAP_CONSTANT_ACCESSOR);
#undef HEAP_CONSTANT_ACCESSOR

#define HEAP_CONSTANT_TEST(rootName, name)         \
  Node* CodeStubAssembler::Is##name(Node* value) { \
    return WordEqual(value, name##Constant());     \
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
    return SmiConstant(Smi::FromInt(value));
  } else {
    DCHECK(mode == INTEGER_PARAMETERS || mode == INTPTR_PARAMETERS);
    return IntPtrConstant(value);
  }
}

Node* CodeStubAssembler::IntPtrAddFoldConstants(Node* left, Node* right) {
  int32_t left_constant;
  bool is_left_constant = ToInt32Constant(left, left_constant);
  int32_t right_constant;
  bool is_right_constant = ToInt32Constant(right, right_constant);
  if (is_left_constant) {
    if (is_right_constant) {
      return IntPtrConstant(left_constant + right_constant);
    }
    if (left_constant == 0) {
      return right;
    }
  } else if (is_right_constant) {
    if (right_constant == 0) {
      return left;
    }
  }
  return IntPtrAdd(left, right);
}

Node* CodeStubAssembler::IntPtrSubFoldConstants(Node* left, Node* right) {
  int32_t left_constant;
  bool is_left_constant = ToInt32Constant(left, left_constant);
  int32_t right_constant;
  bool is_right_constant = ToInt32Constant(right, right_constant);
  if (is_left_constant) {
    if (is_right_constant) {
      return IntPtrConstant(left_constant - right_constant);
    }
  } else if (is_right_constant) {
    if (right_constant == 0) {
      return left;
    }
  }
  return IntPtrSub(left, right);
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

Node* CodeStubAssembler::WordIsPowerOfTwo(Node* value) {
  // value && !(value & (value - 1))
  return WordEqual(
      Select(WordEqual(value, IntPtrConstant(0)), IntPtrConstant(1),
             WordAnd(value, IntPtrSub(value, IntPtrConstant(1))),
             MachineType::PointerRepresentation()),
      IntPtrConstant(0));
}

Node* CodeStubAssembler::Float64Round(Node* x) {
  Node* one = Float64Constant(1.0);
  Node* one_half = Float64Constant(0.5);

  Variable var_x(this, MachineRepresentation::kFloat64);
  Label return_x(this);

  // Round up {x} towards Infinity.
  var_x.Bind(Float64Ceil(x));

  GotoIf(Float64LessThanOrEqual(Float64Sub(var_x.value(), one_half), x),
         &return_x);
  var_x.Bind(Float64Sub(var_x.value(), one));
  Goto(&return_x);

  Bind(&return_x);
  return var_x.value();
}

Node* CodeStubAssembler::Float64Ceil(Node* x) {
  if (IsFloat64RoundUpSupported()) {
    return Float64RoundUp(x);
  }

  Node* one = Float64Constant(1.0);
  Node* zero = Float64Constant(0.0);
  Node* two_52 = Float64Constant(4503599627370496.0E0);
  Node* minus_two_52 = Float64Constant(-4503599627370496.0E0);

  Variable var_x(this, MachineRepresentation::kFloat64);
  Label return_x(this), return_minus_x(this);
  var_x.Bind(x);

  // Check if {x} is greater than zero.
  Label if_xgreaterthanzero(this), if_xnotgreaterthanzero(this);
  Branch(Float64GreaterThan(x, zero), &if_xgreaterthanzero,
         &if_xnotgreaterthanzero);

  Bind(&if_xgreaterthanzero);
  {
    // Just return {x} unless it's in the range ]0,2^52[.
    GotoIf(Float64GreaterThanOrEqual(x, two_52), &return_x);

    // Round positive {x} towards Infinity.
    var_x.Bind(Float64Sub(Float64Add(two_52, x), two_52));
    GotoUnless(Float64LessThan(var_x.value(), x), &return_x);
    var_x.Bind(Float64Add(var_x.value(), one));
    Goto(&return_x);
  }

  Bind(&if_xnotgreaterthanzero);
  {
    // Just return {x} unless it's in the range ]-2^52,0[
    GotoIf(Float64LessThanOrEqual(x, minus_two_52), &return_x);
    GotoUnless(Float64LessThan(x, zero), &return_x);

    // Round negated {x} towards Infinity and return the result negated.
    Node* minus_x = Float64Neg(x);
    var_x.Bind(Float64Sub(Float64Add(two_52, minus_x), two_52));
    GotoUnless(Float64GreaterThan(var_x.value(), minus_x), &return_minus_x);
    var_x.Bind(Float64Sub(var_x.value(), one));
    Goto(&return_minus_x);
  }

  Bind(&return_minus_x);
  var_x.Bind(Float64Neg(var_x.value()));
  Goto(&return_x);

  Bind(&return_x);
  return var_x.value();
}

Node* CodeStubAssembler::Float64Floor(Node* x) {
  if (IsFloat64RoundDownSupported()) {
    return Float64RoundDown(x);
  }

  Node* one = Float64Constant(1.0);
  Node* zero = Float64Constant(0.0);
  Node* two_52 = Float64Constant(4503599627370496.0E0);
  Node* minus_two_52 = Float64Constant(-4503599627370496.0E0);

  Variable var_x(this, MachineRepresentation::kFloat64);
  Label return_x(this), return_minus_x(this);
  var_x.Bind(x);

  // Check if {x} is greater than zero.
  Label if_xgreaterthanzero(this), if_xnotgreaterthanzero(this);
  Branch(Float64GreaterThan(x, zero), &if_xgreaterthanzero,
         &if_xnotgreaterthanzero);

  Bind(&if_xgreaterthanzero);
  {
    // Just return {x} unless it's in the range ]0,2^52[.
    GotoIf(Float64GreaterThanOrEqual(x, two_52), &return_x);

    // Round positive {x} towards -Infinity.
    var_x.Bind(Float64Sub(Float64Add(two_52, x), two_52));
    GotoUnless(Float64GreaterThan(var_x.value(), x), &return_x);
    var_x.Bind(Float64Sub(var_x.value(), one));
    Goto(&return_x);
  }

  Bind(&if_xnotgreaterthanzero);
  {
    // Just return {x} unless it's in the range ]-2^52,0[
    GotoIf(Float64LessThanOrEqual(x, minus_two_52), &return_x);
    GotoUnless(Float64LessThan(x, zero), &return_x);

    // Round negated {x} towards -Infinity and return the result negated.
    Node* minus_x = Float64Neg(x);
    var_x.Bind(Float64Sub(Float64Add(two_52, minus_x), two_52));
    GotoUnless(Float64LessThan(var_x.value(), minus_x), &return_minus_x);
    var_x.Bind(Float64Add(var_x.value(), one));
    Goto(&return_minus_x);
  }

  Bind(&return_minus_x);
  var_x.Bind(Float64Neg(var_x.value()));
  Goto(&return_x);

  Bind(&return_x);
  return var_x.value();
}

Node* CodeStubAssembler::Float64RoundToEven(Node* x) {
  if (IsFloat64RoundTiesEvenSupported()) {
    return Float64RoundTiesEven(x);
  }
  // See ES#sec-touint8clamp for details.
  Node* f = Float64Floor(x);
  Node* f_and_half = Float64Add(f, Float64Constant(0.5));

  Variable var_result(this, MachineRepresentation::kFloat64);
  Label return_f(this), return_f_plus_one(this), done(this);

  GotoIf(Float64LessThan(f_and_half, x), &return_f_plus_one);
  GotoIf(Float64LessThan(x, f_and_half), &return_f);
  {
    Node* f_mod_2 = Float64Mod(f, Float64Constant(2.0));
    Branch(Float64Equal(f_mod_2, Float64Constant(0.0)), &return_f,
           &return_f_plus_one);
  }

  Bind(&return_f);
  var_result.Bind(f);
  Goto(&done);

  Bind(&return_f_plus_one);
  var_result.Bind(Float64Add(f, Float64Constant(1.0)));
  Goto(&done);

  Bind(&done);
  return var_result.value();
}

Node* CodeStubAssembler::Float64Trunc(Node* x) {
  if (IsFloat64RoundTruncateSupported()) {
    return Float64RoundTruncate(x);
  }

  Node* one = Float64Constant(1.0);
  Node* zero = Float64Constant(0.0);
  Node* two_52 = Float64Constant(4503599627370496.0E0);
  Node* minus_two_52 = Float64Constant(-4503599627370496.0E0);

  Variable var_x(this, MachineRepresentation::kFloat64);
  Label return_x(this), return_minus_x(this);
  var_x.Bind(x);

  // Check if {x} is greater than 0.
  Label if_xgreaterthanzero(this), if_xnotgreaterthanzero(this);
  Branch(Float64GreaterThan(x, zero), &if_xgreaterthanzero,
         &if_xnotgreaterthanzero);

  Bind(&if_xgreaterthanzero);
  {
    if (IsFloat64RoundDownSupported()) {
      var_x.Bind(Float64RoundDown(x));
    } else {
      // Just return {x} unless it's in the range ]0,2^52[.
      GotoIf(Float64GreaterThanOrEqual(x, two_52), &return_x);

      // Round positive {x} towards -Infinity.
      var_x.Bind(Float64Sub(Float64Add(two_52, x), two_52));
      GotoUnless(Float64GreaterThan(var_x.value(), x), &return_x);
      var_x.Bind(Float64Sub(var_x.value(), one));
    }
    Goto(&return_x);
  }

  Bind(&if_xnotgreaterthanzero);
  {
    if (IsFloat64RoundUpSupported()) {
      var_x.Bind(Float64RoundUp(x));
      Goto(&return_x);
    } else {
      // Just return {x} unless its in the range ]-2^52,0[.
      GotoIf(Float64LessThanOrEqual(x, minus_two_52), &return_x);
      GotoUnless(Float64LessThan(x, zero), &return_x);

      // Round negated {x} towards -Infinity and return result negated.
      Node* minus_x = Float64Neg(x);
      var_x.Bind(Float64Sub(Float64Add(two_52, minus_x), two_52));
      GotoUnless(Float64GreaterThan(var_x.value(), minus_x), &return_minus_x);
      var_x.Bind(Float64Sub(var_x.value(), one));
      Goto(&return_minus_x);
    }
  }

  Bind(&return_minus_x);
  var_x.Bind(Float64Neg(var_x.value()));
  Goto(&return_x);

  Bind(&return_x);
  return var_x.value();
}

Node* CodeStubAssembler::SmiShiftBitsConstant() {
  return IntPtrConstant(kSmiShiftSize + kSmiTagSize);
}

Node* CodeStubAssembler::SmiFromWord32(Node* value) {
  value = ChangeInt32ToIntPtr(value);
  return BitcastWordToTaggedSigned(WordShl(value, SmiShiftBitsConstant()));
}

Node* CodeStubAssembler::SmiTag(Node* value) {
  int32_t constant_value;
  if (ToInt32Constant(value, constant_value) && Smi::IsValid(constant_value)) {
    return SmiConstant(Smi::FromInt(constant_value));
  }
  return BitcastWordToTaggedSigned(WordShl(value, SmiShiftBitsConstant()));
}

Node* CodeStubAssembler::SmiUntag(Node* value) {
  return WordSar(BitcastTaggedToWord(value), SmiShiftBitsConstant());
}

Node* CodeStubAssembler::SmiToWord32(Node* value) {
  Node* result = SmiUntag(value);
  if (Is64()) {
    result = TruncateInt64ToInt32(result);
  }
  return result;
}

Node* CodeStubAssembler::SmiToFloat64(Node* value) {
  return ChangeInt32ToFloat64(SmiToWord32(value));
}

Node* CodeStubAssembler::SmiAdd(Node* a, Node* b) {
  return BitcastWordToTaggedSigned(
      IntPtrAdd(BitcastTaggedToWord(a), BitcastTaggedToWord(b)));
}

Node* CodeStubAssembler::SmiSub(Node* a, Node* b) {
  return BitcastWordToTaggedSigned(
      IntPtrSub(BitcastTaggedToWord(a), BitcastTaggedToWord(b)));
}

Node* CodeStubAssembler::SmiEqual(Node* a, Node* b) {
  return WordEqual(BitcastTaggedToWord(a), BitcastTaggedToWord(b));
}

Node* CodeStubAssembler::SmiAbove(Node* a, Node* b) {
  return UintPtrGreaterThan(BitcastTaggedToWord(a), BitcastTaggedToWord(b));
}

Node* CodeStubAssembler::SmiAboveOrEqual(Node* a, Node* b) {
  return UintPtrGreaterThanOrEqual(BitcastTaggedToWord(a),
                                   BitcastTaggedToWord(b));
}

Node* CodeStubAssembler::SmiBelow(Node* a, Node* b) {
  return UintPtrLessThan(BitcastTaggedToWord(a), BitcastTaggedToWord(b));
}

Node* CodeStubAssembler::SmiLessThan(Node* a, Node* b) {
  return IntPtrLessThan(BitcastTaggedToWord(a), BitcastTaggedToWord(b));
}

Node* CodeStubAssembler::SmiLessThanOrEqual(Node* a, Node* b) {
  return IntPtrLessThanOrEqual(BitcastTaggedToWord(a), BitcastTaggedToWord(b));
}

Node* CodeStubAssembler::SmiMax(Node* a, Node* b) {
  return Select(SmiLessThan(a, b), b, a);
}

Node* CodeStubAssembler::SmiMin(Node* a, Node* b) {
  return Select(SmiLessThan(a, b), a, b);
}

Node* CodeStubAssembler::SmiMod(Node* a, Node* b) {
  Variable var_result(this, MachineRepresentation::kTagged);
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

  Bind(&if_aisnotnegative);
  {
    // Fast case, don't need to check any other edge cases.
    Node* r = Int32Mod(a, b);
    var_result.Bind(SmiFromWord32(r));
    Goto(&return_result);
  }

  Bind(&if_aisnegative);
  {
    if (SmiValuesAre32Bits()) {
      // Check if {a} is kMinInt and {b} is -1 (only relevant if the
      // kMinInt is actually representable as a Smi).
      Label join(this);
      GotoUnless(Word32Equal(a, Int32Constant(kMinInt)), &join);
      GotoIf(Word32Equal(b, Int32Constant(-1)), &return_minuszero);
      Goto(&join);
      Bind(&join);
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

  Bind(&return_minuszero);
  var_result.Bind(MinusZeroConstant());
  Goto(&return_result);

  Bind(&return_nan);
  var_result.Bind(NanConstant());
  Goto(&return_result);

  Bind(&return_result);
  return var_result.value();
}

Node* CodeStubAssembler::SmiMul(Node* a, Node* b) {
  Variable var_result(this, MachineRepresentation::kTagged);
  Variable var_lhs_float64(this, MachineRepresentation::kFloat64),
      var_rhs_float64(this, MachineRepresentation::kFloat64);
  Label return_result(this, &var_result);

  // Both {a} and {b} are Smis. Convert them to integers and multiply.
  Node* lhs32 = SmiToWord32(a);
  Node* rhs32 = SmiToWord32(b);
  Node* pair = Int32MulWithOverflow(lhs32, rhs32);

  Node* overflow = Projection(1, pair);

  // Check if the multiplication overflowed.
  Label if_overflow(this, Label::kDeferred), if_notoverflow(this);
  Branch(overflow, &if_overflow, &if_notoverflow);
  Bind(&if_notoverflow);
  {
    // If the answer is zero, we may need to return -0.0, depending on the
    // input.
    Label answer_zero(this), answer_not_zero(this);
    Node* answer = Projection(0, pair);
    Node* zero = Int32Constant(0);
    Branch(WordEqual(answer, zero), &answer_zero, &answer_not_zero);
    Bind(&answer_not_zero);
    {
      var_result.Bind(ChangeInt32ToTagged(answer));
      Goto(&return_result);
    }
    Bind(&answer_zero);
    {
      Node* or_result = Word32Or(lhs32, rhs32);
      Label if_should_be_negative_zero(this), if_should_be_zero(this);
      Branch(Int32LessThan(or_result, zero), &if_should_be_negative_zero,
             &if_should_be_zero);
      Bind(&if_should_be_negative_zero);
      {
        var_result.Bind(MinusZeroConstant());
        Goto(&return_result);
      }
      Bind(&if_should_be_zero);
      {
        var_result.Bind(zero);
        Goto(&return_result);
      }
    }
  }
  Bind(&if_overflow);
  {
    var_lhs_float64.Bind(SmiToFloat64(a));
    var_rhs_float64.Bind(SmiToFloat64(b));
    Node* value = Float64Mul(var_lhs_float64.value(), var_rhs_float64.value());
    Node* result = AllocateHeapNumberWithValue(value);
    var_result.Bind(result);
    Goto(&return_result);
  }

  Bind(&return_result);
  return var_result.value();
}

Node* CodeStubAssembler::TaggedIsSmi(Node* a) {
  return WordEqual(WordAnd(BitcastTaggedToWord(a), IntPtrConstant(kSmiTagMask)),
                   IntPtrConstant(0));
}

Node* CodeStubAssembler::WordIsPositiveSmi(Node* a) {
  return WordEqual(WordAnd(a, IntPtrConstant(kSmiTagMask | kSmiSignMask)),
                   IntPtrConstant(0));
}

Node* CodeStubAssembler::WordIsWordAligned(Node* word) {
  return WordEqual(IntPtrConstant(0),
                   WordAnd(word, IntPtrConstant((1 << kPointerSizeLog2) - 1)));
}

void CodeStubAssembler::BranchIfSimd128Equal(Node* lhs, Node* lhs_map,
                                             Node* rhs, Node* rhs_map,
                                             Label* if_equal,
                                             Label* if_notequal) {
  Label if_mapsame(this), if_mapnotsame(this);
  Branch(WordEqual(lhs_map, rhs_map), &if_mapsame, &if_mapnotsame);

  Bind(&if_mapsame);
  {
    // Both {lhs} and {rhs} are Simd128Values with the same map, need special
    // handling for Float32x4 because of NaN comparisons.
    Label if_float32x4(this), if_notfloat32x4(this);
    Node* float32x4_map = HeapConstant(factory()->float32x4_map());
    Branch(WordEqual(lhs_map, float32x4_map), &if_float32x4, &if_notfloat32x4);

    Bind(&if_float32x4);
    {
      // Both {lhs} and {rhs} are Float32x4, compare the lanes individually
      // using a floating point comparison.
      for (int offset = Float32x4::kValueOffset - kHeapObjectTag;
           offset < Float32x4::kSize - kHeapObjectTag;
           offset += sizeof(float)) {
        // Load the floating point values for {lhs} and {rhs}.
        Node* lhs_value =
            Load(MachineType::Float32(), lhs, IntPtrConstant(offset));
        Node* rhs_value =
            Load(MachineType::Float32(), rhs, IntPtrConstant(offset));

        // Perform a floating point comparison.
        Label if_valueequal(this), if_valuenotequal(this);
        Branch(Float32Equal(lhs_value, rhs_value), &if_valueequal,
               &if_valuenotequal);
        Bind(&if_valuenotequal);
        Goto(if_notequal);
        Bind(&if_valueequal);
      }

      // All 4 lanes match, {lhs} and {rhs} considered equal.
      Goto(if_equal);
    }

    Bind(&if_notfloat32x4);
    {
      // For other Simd128Values we just perform a bitwise comparison.
      for (int offset = Simd128Value::kValueOffset - kHeapObjectTag;
           offset < Simd128Value::kSize - kHeapObjectTag;
           offset += kPointerSize) {
        // Load the word values for {lhs} and {rhs}.
        Node* lhs_value =
            Load(MachineType::Pointer(), lhs, IntPtrConstant(offset));
        Node* rhs_value =
            Load(MachineType::Pointer(), rhs, IntPtrConstant(offset));

        // Perform a bitwise word-comparison.
        Label if_valueequal(this), if_valuenotequal(this);
        Branch(WordEqual(lhs_value, rhs_value), &if_valueequal,
               &if_valuenotequal);
        Bind(&if_valuenotequal);
        Goto(if_notequal);
        Bind(&if_valueequal);
      }

      // Bitwise comparison succeeded, {lhs} and {rhs} considered equal.
      Goto(if_equal);
    }
  }

  Bind(&if_mapnotsame);
  Goto(if_notequal);
}

void CodeStubAssembler::BranchIfPrototypesHaveNoElements(
    Node* receiver_map, Label* definitely_no_elements,
    Label* possibly_elements) {
  Variable var_map(this, MachineRepresentation::kTagged);
  var_map.Bind(receiver_map);
  Label loop_body(this, &var_map);
  Node* empty_elements = LoadRoot(Heap::kEmptyFixedArrayRootIndex);
  Goto(&loop_body);

  Bind(&loop_body);
  {
    Node* map = var_map.value();
    Node* prototype = LoadMapPrototype(map);
    GotoIf(WordEqual(prototype, NullConstant()), definitely_no_elements);
    Node* prototype_map = LoadMap(prototype);
    // Pessimistically assume elements if a Proxy, Special API Object,
    // or JSValue wrapper is found on the prototype chain. After this
    // instance type check, it's not necessary to check for interceptors or
    // access checks.
    GotoIf(Int32LessThanOrEqual(LoadMapInstanceType(prototype_map),
                                Int32Constant(LAST_CUSTOM_ELEMENTS_RECEIVER)),
           possibly_elements);
    GotoIf(WordNotEqual(LoadElements(prototype), empty_elements),
           possibly_elements);
    var_map.Bind(prototype_map);
    Goto(&loop_body);
  }
}

void CodeStubAssembler::BranchIfJSReceiver(Node* object, Label* if_true,
                                           Label* if_false) {
  GotoIf(TaggedIsSmi(object), if_false);
  STATIC_ASSERT(LAST_JS_RECEIVER_TYPE == LAST_TYPE);
  Branch(Int32GreaterThanOrEqual(LoadInstanceType(object),
                                 Int32Constant(FIRST_JS_RECEIVER_TYPE)),
         if_true, if_false);
}

void CodeStubAssembler::BranchIfJSObject(Node* object, Label* if_true,
                                         Label* if_false) {
  GotoIf(TaggedIsSmi(object), if_false);
  STATIC_ASSERT(LAST_JS_OBJECT_TYPE == LAST_TYPE);
  Branch(Int32GreaterThanOrEqual(LoadInstanceType(object),
                                 Int32Constant(FIRST_JS_OBJECT_TYPE)),
         if_true, if_false);
}

void CodeStubAssembler::BranchIfFastJSArray(Node* object, Node* context,
                                            Label* if_true, Label* if_false) {
  // Bailout if receiver is a Smi.
  GotoIf(TaggedIsSmi(object), if_false);

  Node* map = LoadMap(object);

  // Bailout if instance type is not JS_ARRAY_TYPE.
  GotoIf(WordNotEqual(LoadMapInstanceType(map), Int32Constant(JS_ARRAY_TYPE)),
         if_false);

  Node* elements_kind = LoadMapElementsKind(map);

  // Bailout if receiver has slow elements.
  GotoUnless(IsFastElementsKind(elements_kind), if_false);

  // Check prototype chain if receiver does not have packed elements.
  GotoUnless(IsHoleyFastElementsKind(elements_kind), if_true);

  BranchIfPrototypesHaveNoElements(map, if_true, if_false);
}

Node* CodeStubAssembler::AllocateRawUnaligned(Node* size_in_bytes,
                                              AllocationFlags flags,
                                              Node* top_address,
                                              Node* limit_address) {
  Node* top = Load(MachineType::Pointer(), top_address);
  Node* limit = Load(MachineType::Pointer(), limit_address);

  // If there's not enough space, call the runtime.
  Variable result(this, MachineRepresentation::kTagged);
  Label runtime_call(this, Label::kDeferred), no_runtime_call(this);
  Label merge_runtime(this, &result);

  Node* new_top = IntPtrAdd(top, size_in_bytes);
  Branch(UintPtrGreaterThanOrEqual(new_top, limit), &runtime_call,
         &no_runtime_call);

  Bind(&runtime_call);
  Node* runtime_result;
  if (flags & kPretenured) {
    Node* runtime_flags = SmiConstant(
        Smi::FromInt(AllocateDoubleAlignFlag::encode(false) |
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
  Bind(&no_runtime_call);
  Node* no_runtime_result = top;
  StoreNoWriteBarrier(MachineType::PointerRepresentation(), top_address,
                      new_top);
  no_runtime_result = BitcastWordToTagged(
      IntPtrAdd(no_runtime_result, IntPtrConstant(kHeapObjectTag)));
  result.Bind(no_runtime_result);
  Goto(&merge_runtime);

  Bind(&merge_runtime);
  return result.value();
}

Node* CodeStubAssembler::AllocateRawAligned(Node* size_in_bytes,
                                            AllocationFlags flags,
                                            Node* top_address,
                                            Node* limit_address) {
  Node* top = Load(MachineType::Pointer(), top_address);
  Node* limit = Load(MachineType::Pointer(), limit_address);
  Variable adjusted_size(this, MachineType::PointerRepresentation());
  adjusted_size.Bind(size_in_bytes);
  if (flags & kDoubleAlignment) {
    // TODO(epertoso): Simd128 alignment.
    Label aligned(this), not_aligned(this), merge(this, &adjusted_size);
    Branch(WordAnd(top, IntPtrConstant(kDoubleAlignmentMask)), &not_aligned,
           &aligned);

    Bind(&not_aligned);
    Node* not_aligned_size =
        IntPtrAdd(size_in_bytes, IntPtrConstant(kPointerSize));
    adjusted_size.Bind(not_aligned_size);
    Goto(&merge);

    Bind(&aligned);
    Goto(&merge);

    Bind(&merge);
  }

  Variable address(this, MachineRepresentation::kTagged);
  address.Bind(AllocateRawUnaligned(adjusted_size.value(), kNone, top, limit));

  Label needs_filler(this), doesnt_need_filler(this),
      merge_address(this, &address);
  Branch(IntPtrEqual(adjusted_size.value(), size_in_bytes), &doesnt_need_filler,
         &needs_filler);

  Bind(&needs_filler);
  // Store a filler and increase the address by kPointerSize.
  // TODO(epertoso): this code assumes that we only align to kDoubleSize. Change
  // it when Simd128 alignment is supported.
  StoreNoWriteBarrier(MachineType::PointerRepresentation(), top,
                      LoadRoot(Heap::kOnePointerFillerMapRootIndex));
  address.Bind(BitcastWordToTagged(
      IntPtrAdd(address.value(), IntPtrConstant(kPointerSize))));
  Goto(&merge_address);

  Bind(&doesnt_need_filler);
  Goto(&merge_address);

  Bind(&merge_address);
  // Update the top.
  StoreNoWriteBarrier(MachineType::PointerRepresentation(), top_address,
                      IntPtrAdd(top, adjusted_size.value()));
  return address.value();
}

Node* CodeStubAssembler::Allocate(Node* size_in_bytes, AllocationFlags flags) {
  Comment("Allocate");
  bool const new_space = !(flags & kPretenured);
  Node* top_address = ExternalConstant(
      new_space
          ? ExternalReference::new_space_allocation_top_address(isolate())
          : ExternalReference::old_space_allocation_top_address(isolate()));
  Node* limit_address = ExternalConstant(
      new_space
          ? ExternalReference::new_space_allocation_limit_address(isolate())
          : ExternalReference::old_space_allocation_limit_address(isolate()));

#ifdef V8_HOST_ARCH_32_BIT
  if (flags & kDoubleAlignment) {
    return AllocateRawAligned(size_in_bytes, flags, top_address, limit_address);
  }
#endif

  return AllocateRawUnaligned(size_in_bytes, flags, top_address, limit_address);
}

Node* CodeStubAssembler::Allocate(int size_in_bytes, AllocationFlags flags) {
  return CodeStubAssembler::Allocate(IntPtrConstant(size_in_bytes), flags);
}

Node* CodeStubAssembler::InnerAllocate(Node* previous, Node* offset) {
  return BitcastWordToTagged(IntPtrAdd(previous, offset));
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
  Label if_valueissmi(this), if_valueisnotsmi(this), if_valueisstring(this),
      if_valueisheapnumber(this), if_valueisother(this);

  // Fast check for Boolean {value}s (common case).
  GotoIf(WordEqual(value, BooleanConstant(true)), if_true);
  GotoIf(WordEqual(value, BooleanConstant(false)), if_false);

  // Check if {value} is a Smi or a HeapObject.
  Branch(TaggedIsSmi(value), &if_valueissmi, &if_valueisnotsmi);

  Bind(&if_valueissmi);
  {
    // The {value} is a Smi, only need to check against zero.
    BranchIfSmiEqual(value, SmiConstant(0), if_false, if_true);
  }

  Bind(&if_valueisnotsmi);
  {
    // The {value} is a HeapObject, load its map.
    Node* value_map = LoadMap(value);

    // Load the {value}s instance type.
    Node* value_instance_type = LoadMapInstanceType(value_map);

    // Dispatch based on the instance type; we distinguish all String instance
    // types, the HeapNumber type and everything else.
    GotoIf(Word32Equal(value_instance_type, Int32Constant(HEAP_NUMBER_TYPE)),
           &if_valueisheapnumber);
    Branch(IsStringInstanceType(value_instance_type), &if_valueisstring,
           &if_valueisother);

    Bind(&if_valueisstring);
    {
      // Load the string length field of the {value}.
      Node* value_length = LoadObjectField(value, String::kLengthOffset);

      // Check if the {value} is the empty string.
      BranchIfSmiEqual(value_length, SmiConstant(0), if_false, if_true);
    }

    Bind(&if_valueisheapnumber);
    {
      // Load the floating point value of {value}.
      Node* value_value = LoadObjectField(value, HeapNumber::kValueOffset,
                                          MachineType::Float64());

      // Check if the floating point {value} is neither 0.0, -0.0 nor NaN.
      Branch(Float64LessThan(Float64Constant(0.0), Float64Abs(value_value)),
             if_true, if_false);
    }

    Bind(&if_valueisother);
    {
      // Load the bit field from the {value}s map. The {value} is now either
      // Null or Undefined, which have the undetectable bit set (so we always
      // return false for those), or a Symbol or Simd128Value, whose maps never
      // have the undetectable bit set (so we always return true for those), or
      // a JSReceiver, which may or may not have the undetectable bit set.
      Node* value_map_bitfield = LoadMapBitField(value_map);
      Node* value_map_undetectable = Word32And(
          value_map_bitfield, Int32Constant(1 << Map::kIsUndetectable));

      // Check if the {value} is undetectable.
      Branch(Word32Equal(value_map_undetectable, Int32Constant(0)), if_true,
             if_false);
    }
  }
}

compiler::Node* CodeStubAssembler::LoadFromFrame(int offset, MachineType rep) {
  Node* frame_pointer = LoadFramePointer();
  return Load(rep, frame_pointer, IntPtrConstant(offset));
}

compiler::Node* CodeStubAssembler::LoadFromParentFrame(int offset,
                                                       MachineType rep) {
  Node* frame_pointer = LoadParentFramePointer();
  return Load(rep, frame_pointer, IntPtrConstant(offset));
}

Node* CodeStubAssembler::LoadBufferObject(Node* buffer, int offset,
                                          MachineType rep) {
  return Load(rep, buffer, IntPtrConstant(offset));
}

Node* CodeStubAssembler::LoadObjectField(Node* object, int offset,
                                         MachineType rep) {
  return Load(rep, object, IntPtrConstant(offset - kHeapObjectTag));
}

Node* CodeStubAssembler::LoadObjectField(Node* object, Node* offset,
                                         MachineType rep) {
  return Load(rep, object, IntPtrSub(offset, IntPtrConstant(kHeapObjectTag)));
}

Node* CodeStubAssembler::LoadAndUntagObjectField(Node* object, int offset) {
  if (Is64()) {
#if V8_TARGET_LITTLE_ENDIAN
    offset += kPointerSize / 2;
#endif
    return ChangeInt32ToInt64(
        LoadObjectField(object, offset, MachineType::Int32()));
  } else {
    return SmiToWord(LoadObjectField(object, offset, MachineType::AnyTagged()));
  }
}

Node* CodeStubAssembler::LoadAndUntagToWord32ObjectField(Node* object,
                                                         int offset) {
  if (Is64()) {
#if V8_TARGET_LITTLE_ENDIAN
    offset += kPointerSize / 2;
#endif
    return LoadObjectField(object, offset, MachineType::Int32());
  } else {
    return SmiToWord32(
        LoadObjectField(object, offset, MachineType::AnyTagged()));
  }
}

Node* CodeStubAssembler::LoadAndUntagSmi(Node* base, int index) {
  if (Is64()) {
#if V8_TARGET_LITTLE_ENDIAN
    index += kPointerSize / 2;
#endif
    return ChangeInt32ToInt64(
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

Node* CodeStubAssembler::LoadHeapNumberValue(Node* object) {
  return LoadObjectField(object, HeapNumber::kValueOffset,
                         MachineType::Float64());
}

Node* CodeStubAssembler::LoadMap(Node* object) {
  return LoadObjectField(object, HeapObject::kMapOffset);
}

Node* CodeStubAssembler::LoadInstanceType(Node* object) {
  return LoadMapInstanceType(LoadMap(object));
}

Node* CodeStubAssembler::HasInstanceType(Node* object,
                                         InstanceType instance_type) {
  return Word32Equal(LoadInstanceType(object), Int32Constant(instance_type));
}

Node* CodeStubAssembler::LoadProperties(Node* object) {
  return LoadObjectField(object, JSObject::kPropertiesOffset);
}

Node* CodeStubAssembler::LoadElements(Node* object) {
  return LoadObjectField(object, JSObject::kElementsOffset);
}

Node* CodeStubAssembler::LoadJSArrayLength(Node* array) {
  CSA_ASSERT(this, IsJSArray(array));
  return LoadObjectField(array, JSArray::kLengthOffset);
}

Node* CodeStubAssembler::LoadFixedArrayBaseLength(Node* array) {
  return LoadObjectField(array, FixedArrayBase::kLengthOffset);
}

Node* CodeStubAssembler::LoadAndUntagFixedArrayBaseLength(Node* array) {
  return LoadAndUntagObjectField(array, FixedArrayBase::kLengthOffset);
}

Node* CodeStubAssembler::LoadMapBitField(Node* map) {
  CSA_SLOW_ASSERT(this, IsMap(map));
  return LoadObjectField(map, Map::kBitFieldOffset, MachineType::Uint8());
}

Node* CodeStubAssembler::LoadMapBitField2(Node* map) {
  CSA_SLOW_ASSERT(this, IsMap(map));
  return LoadObjectField(map, Map::kBitField2Offset, MachineType::Uint8());
}

Node* CodeStubAssembler::LoadMapBitField3(Node* map) {
  CSA_SLOW_ASSERT(this, IsMap(map));
  return LoadObjectField(map, Map::kBitField3Offset, MachineType::Uint32());
}

Node* CodeStubAssembler::LoadMapInstanceType(Node* map) {
  return LoadObjectField(map, Map::kInstanceTypeOffset, MachineType::Uint8());
}

Node* CodeStubAssembler::LoadMapElementsKind(Node* map) {
  CSA_SLOW_ASSERT(this, IsMap(map));
  Node* bit_field2 = LoadMapBitField2(map);
  return DecodeWord32<Map::ElementsKindBits>(bit_field2);
}

Node* CodeStubAssembler::LoadMapDescriptors(Node* map) {
  CSA_SLOW_ASSERT(this, IsMap(map));
  return LoadObjectField(map, Map::kDescriptorsOffset);
}

Node* CodeStubAssembler::LoadMapPrototype(Node* map) {
  CSA_SLOW_ASSERT(this, IsMap(map));
  return LoadObjectField(map, Map::kPrototypeOffset);
}

Node* CodeStubAssembler::LoadMapPrototypeInfo(Node* map,
                                              Label* if_no_proto_info) {
  CSA_ASSERT(this, IsMap(map));
  Node* prototype_info =
      LoadObjectField(map, Map::kTransitionsOrPrototypeInfoOffset);
  GotoIf(TaggedIsSmi(prototype_info), if_no_proto_info);
  GotoUnless(WordEqual(LoadMap(prototype_info),
                       LoadRoot(Heap::kPrototypeInfoMapRootIndex)),
             if_no_proto_info);
  return prototype_info;
}

Node* CodeStubAssembler::LoadMapInstanceSize(Node* map) {
  CSA_SLOW_ASSERT(this, IsMap(map));
  return ChangeUint32ToWord(
      LoadObjectField(map, Map::kInstanceSizeOffset, MachineType::Uint8()));
}

Node* CodeStubAssembler::LoadMapInobjectProperties(Node* map) {
  CSA_SLOW_ASSERT(this, IsMap(map));
  // See Map::GetInObjectProperties() for details.
  STATIC_ASSERT(LAST_JS_OBJECT_TYPE == LAST_TYPE);
  CSA_ASSERT(this,
             Int32GreaterThanOrEqual(LoadMapInstanceType(map),
                                     Int32Constant(FIRST_JS_OBJECT_TYPE)));
  return ChangeUint32ToWord(LoadObjectField(
      map, Map::kInObjectPropertiesOrConstructorFunctionIndexOffset,
      MachineType::Uint8()));
}

Node* CodeStubAssembler::LoadMapConstructorFunctionIndex(Node* map) {
  CSA_SLOW_ASSERT(this, IsMap(map));
  // See Map::GetConstructorFunctionIndex() for details.
  STATIC_ASSERT(FIRST_PRIMITIVE_TYPE == FIRST_TYPE);
  CSA_ASSERT(this, Int32LessThanOrEqual(LoadMapInstanceType(map),
                                        Int32Constant(LAST_PRIMITIVE_TYPE)));
  return ChangeUint32ToWord(LoadObjectField(
      map, Map::kInObjectPropertiesOrConstructorFunctionIndexOffset,
      MachineType::Uint8()));
}

Node* CodeStubAssembler::LoadMapConstructor(Node* map) {
  CSA_SLOW_ASSERT(this, IsMap(map));
  Variable result(this, MachineRepresentation::kTagged);
  result.Bind(LoadObjectField(map, Map::kConstructorOrBackPointerOffset));

  Label done(this), loop(this, &result);
  Goto(&loop);
  Bind(&loop);
  {
    GotoIf(TaggedIsSmi(result.value()), &done);
    Node* is_map_type =
        Word32Equal(LoadInstanceType(result.value()), Int32Constant(MAP_TYPE));
    GotoUnless(is_map_type, &done);
    result.Bind(
        LoadObjectField(result.value(), Map::kConstructorOrBackPointerOffset));
    Goto(&loop);
  }
  Bind(&done);
  return result.value();
}

Node* CodeStubAssembler::LoadNameHashField(Node* name) {
  CSA_ASSERT(this, IsName(name));
  return LoadObjectField(name, Name::kHashFieldOffset, MachineType::Uint32());
}

Node* CodeStubAssembler::LoadNameHash(Node* name, Label* if_hash_not_computed) {
  Node* hash_field = LoadNameHashField(name);
  if (if_hash_not_computed != nullptr) {
    GotoIf(Word32Equal(
               Word32And(hash_field, Int32Constant(Name::kHashNotComputedMask)),
               Int32Constant(0)),
           if_hash_not_computed);
  }
  return Word32Shr(hash_field, Int32Constant(Name::kHashShift));
}

Node* CodeStubAssembler::LoadStringLength(Node* object) {
  CSA_ASSERT(this, IsString(object));
  return LoadObjectField(object, String::kLengthOffset);
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
  int32_t header_size =
      FixedArray::kHeaderSize + additional_offset - kHeapObjectTag;
  Node* offset = ElementOffsetFromIndex(index_node, FAST_HOLEY_ELEMENTS,
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

Node* CodeStubAssembler::LoadAndUntagToWord32FixedArrayElement(
    Node* object, Node* index_node, int additional_offset,
    ParameterMode parameter_mode) {
  int32_t header_size =
      FixedArray::kHeaderSize + additional_offset - kHeapObjectTag;
#if V8_TARGET_LITTLE_ENDIAN
  if (Is64()) {
    header_size += kPointerSize / 2;
  }
#endif
  Node* offset = ElementOffsetFromIndex(index_node, FAST_HOLEY_ELEMENTS,
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
  CSA_ASSERT(this, IsFixedDoubleArray(object));
  int32_t header_size =
      FixedDoubleArray::kHeaderSize + additional_offset - kHeapObjectTag;
  Node* offset = ElementOffsetFromIndex(index_node, FAST_HOLEY_DOUBLE_ELEMENTS,
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

Node* CodeStubAssembler::LoadContextElement(Node* context, int slot_index) {
  int offset = Context::SlotOffset(slot_index);
  return Load(MachineType::AnyTagged(), context, IntPtrConstant(offset));
}

Node* CodeStubAssembler::LoadContextElement(Node* context, Node* slot_index) {
  Node* offset =
      IntPtrAdd(WordShl(slot_index, kPointerSizeLog2),
                IntPtrConstant(Context::kHeaderSize - kHeapObjectTag));
  return Load(MachineType::AnyTagged(), context, offset);
}

Node* CodeStubAssembler::StoreContextElement(Node* context, int slot_index,
                                             Node* value) {
  int offset = Context::SlotOffset(slot_index);
  return Store(MachineRepresentation::kTagged, context, IntPtrConstant(offset),
               value);
}

Node* CodeStubAssembler::StoreContextElement(Node* context, Node* slot_index,
                                             Node* value) {
  Node* offset =
      IntPtrAdd(WordShl(slot_index, kPointerSizeLog2),
                IntPtrConstant(Context::kHeaderSize - kHeapObjectTag));
  return Store(MachineRepresentation::kTagged, context, offset, value);
}

Node* CodeStubAssembler::LoadNativeContext(Node* context) {
  return LoadContextElement(context, Context::NATIVE_CONTEXT_INDEX);
}

Node* CodeStubAssembler::LoadJSArrayElementsMap(ElementsKind kind,
                                                Node* native_context) {
  CSA_ASSERT(this, IsNativeContext(native_context));
  return LoadFixedArrayElement(native_context,
                               IntPtrConstant(Context::ArrayMapIndex(kind)));
}

Node* CodeStubAssembler::StoreHeapNumberValue(Node* object, Node* value) {
  return StoreObjectFieldNoWriteBarrier(object, HeapNumber::kValueOffset, value,
                                        MachineRepresentation::kFloat64);
}

Node* CodeStubAssembler::StoreObjectField(
    Node* object, int offset, Node* value) {
  return Store(MachineRepresentation::kTagged, object,
               IntPtrConstant(offset - kHeapObjectTag), value);
}

Node* CodeStubAssembler::StoreObjectField(Node* object, Node* offset,
                                          Node* value) {
  int const_offset;
  if (ToInt32Constant(offset, const_offset)) {
    return StoreObjectField(object, const_offset, value);
  }
  return Store(MachineRepresentation::kTagged, object,
               IntPtrSub(offset, IntPtrConstant(kHeapObjectTag)), value);
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

Node* CodeStubAssembler::StoreMapNoWriteBarrier(Node* object, Node* map) {
  return StoreNoWriteBarrier(
      MachineRepresentation::kTagged, object,
      IntPtrConstant(HeapNumber::kMapOffset - kHeapObjectTag), map);
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
                                                ParameterMode parameter_mode) {
  DCHECK(barrier_mode == SKIP_WRITE_BARRIER ||
         barrier_mode == UPDATE_WRITE_BARRIER);
  Node* offset =
      ElementOffsetFromIndex(index_node, FAST_HOLEY_ELEMENTS, parameter_mode,
                             FixedArray::kHeaderSize - kHeapObjectTag);
  MachineRepresentation rep = MachineRepresentation::kTagged;
  if (barrier_mode == SKIP_WRITE_BARRIER) {
    return StoreNoWriteBarrier(rep, object, offset, value);
  } else {
    return Store(rep, object, offset, value);
  }
}

Node* CodeStubAssembler::StoreFixedDoubleArrayElement(
    Node* object, Node* index_node, Node* value, ParameterMode parameter_mode) {
  CSA_ASSERT(this, IsFixedDoubleArray(object));
  Node* offset =
      ElementOffsetFromIndex(index_node, FAST_DOUBLE_ELEMENTS, parameter_mode,
                             FixedArray::kHeaderSize - kHeapObjectTag);
  MachineRepresentation rep = MachineRepresentation::kFloat64;
  return StoreNoWriteBarrier(rep, object, offset, value);
}

Node* CodeStubAssembler::AllocateHeapNumber(MutableMode mode) {
  Node* result = Allocate(HeapNumber::kSize, kNone);
  Heap::RootListIndex heap_map_index =
      mode == IMMUTABLE ? Heap::kHeapNumberMapRootIndex
                        : Heap::kMutableHeapNumberMapRootIndex;
  Node* map = LoadRoot(heap_map_index);
  StoreMapNoWriteBarrier(result, map);
  return result;
}

Node* CodeStubAssembler::AllocateHeapNumberWithValue(Node* value,
                                                     MutableMode mode) {
  Node* result = AllocateHeapNumber(mode);
  StoreHeapNumberValue(result, value);
  return result;
}

Node* CodeStubAssembler::AllocateSeqOneByteString(int length,
                                                  AllocationFlags flags) {
  Comment("AllocateSeqOneByteString");
  Node* result = Allocate(SeqOneByteString::SizeFor(length), flags);
  DCHECK(Heap::RootIsImmortalImmovable(Heap::kOneByteStringMapRootIndex));
  StoreMapNoWriteBarrier(result, LoadRoot(Heap::kOneByteStringMapRootIndex));
  StoreObjectFieldNoWriteBarrier(result, SeqOneByteString::kLengthOffset,
                                 SmiConstant(Smi::FromInt(length)));
  StoreObjectFieldNoWriteBarrier(result, SeqOneByteString::kHashFieldOffset,
                                 IntPtrConstant(String::kEmptyHashField),
                                 MachineRepresentation::kWord32);
  return result;
}

Node* CodeStubAssembler::AllocateSeqOneByteString(Node* context, Node* length,
                                                  ParameterMode mode,
                                                  AllocationFlags flags) {
  Comment("AllocateSeqOneByteString");
  Variable var_result(this, MachineRepresentation::kTagged);

  // Compute the SeqOneByteString size and check if it fits into new space.
  Label if_sizeissmall(this), if_notsizeissmall(this, Label::kDeferred),
      if_join(this);
  Node* raw_size = GetArrayAllocationSize(
      length, UINT8_ELEMENTS, mode,
      SeqOneByteString::kHeaderSize + kObjectAlignmentMask);
  Node* size = WordAnd(raw_size, IntPtrConstant(~kObjectAlignmentMask));
  Branch(IntPtrLessThanOrEqual(size, IntPtrConstant(kMaxRegularHeapObjectSize)),
         &if_sizeissmall, &if_notsizeissmall);

  Bind(&if_sizeissmall);
  {
    // Just allocate the SeqOneByteString in new space.
    Node* result = Allocate(size, flags);
    DCHECK(Heap::RootIsImmortalImmovable(Heap::kOneByteStringMapRootIndex));
    StoreMapNoWriteBarrier(result, LoadRoot(Heap::kOneByteStringMapRootIndex));
    StoreObjectFieldNoWriteBarrier(
        result, SeqOneByteString::kLengthOffset,
        mode == SMI_PARAMETERS ? length : SmiFromWord(length));
    StoreObjectFieldNoWriteBarrier(result, SeqOneByteString::kHashFieldOffset,
                                   IntPtrConstant(String::kEmptyHashField),
                                   MachineRepresentation::kWord32);
    var_result.Bind(result);
    Goto(&if_join);
  }

  Bind(&if_notsizeissmall);
  {
    // We might need to allocate in large object space, go to the runtime.
    Node* result =
        CallRuntime(Runtime::kAllocateSeqOneByteString, context,
                    mode == SMI_PARAMETERS ? length : SmiFromWord(length));
    var_result.Bind(result);
    Goto(&if_join);
  }

  Bind(&if_join);
  return var_result.value();
}

Node* CodeStubAssembler::AllocateSeqTwoByteString(int length,
                                                  AllocationFlags flags) {
  Comment("AllocateSeqTwoByteString");
  Node* result = Allocate(SeqTwoByteString::SizeFor(length), flags);
  DCHECK(Heap::RootIsImmortalImmovable(Heap::kStringMapRootIndex));
  StoreMapNoWriteBarrier(result, LoadRoot(Heap::kStringMapRootIndex));
  StoreObjectFieldNoWriteBarrier(result, SeqTwoByteString::kLengthOffset,
                                 SmiConstant(Smi::FromInt(length)));
  StoreObjectFieldNoWriteBarrier(result, SeqTwoByteString::kHashFieldOffset,
                                 IntPtrConstant(String::kEmptyHashField),
                                 MachineRepresentation::kWord32);
  return result;
}

Node* CodeStubAssembler::AllocateSeqTwoByteString(Node* context, Node* length,
                                                  ParameterMode mode,
                                                  AllocationFlags flags) {
  Comment("AllocateSeqTwoByteString");
  Variable var_result(this, MachineRepresentation::kTagged);

  // Compute the SeqTwoByteString size and check if it fits into new space.
  Label if_sizeissmall(this), if_notsizeissmall(this, Label::kDeferred),
      if_join(this);
  Node* raw_size = GetArrayAllocationSize(
      length, UINT16_ELEMENTS, mode,
      SeqOneByteString::kHeaderSize + kObjectAlignmentMask);
  Node* size = WordAnd(raw_size, IntPtrConstant(~kObjectAlignmentMask));
  Branch(IntPtrLessThanOrEqual(size, IntPtrConstant(kMaxRegularHeapObjectSize)),
         &if_sizeissmall, &if_notsizeissmall);

  Bind(&if_sizeissmall);
  {
    // Just allocate the SeqTwoByteString in new space.
    Node* result = Allocate(size, flags);
    DCHECK(Heap::RootIsImmortalImmovable(Heap::kStringMapRootIndex));
    StoreMapNoWriteBarrier(result, LoadRoot(Heap::kStringMapRootIndex));
    StoreObjectFieldNoWriteBarrier(
        result, SeqTwoByteString::kLengthOffset,
        mode == SMI_PARAMETERS ? length : SmiFromWord(length));
    StoreObjectFieldNoWriteBarrier(result, SeqTwoByteString::kHashFieldOffset,
                                   IntPtrConstant(String::kEmptyHashField),
                                   MachineRepresentation::kWord32);
    var_result.Bind(result);
    Goto(&if_join);
  }

  Bind(&if_notsizeissmall);
  {
    // We might need to allocate in large object space, go to the runtime.
    Node* result =
        CallRuntime(Runtime::kAllocateSeqTwoByteString, context,
                    mode == SMI_PARAMETERS ? length : SmiFromWord(length));
    var_result.Bind(result);
    Goto(&if_join);
  }

  Bind(&if_join);
  return var_result.value();
}

Node* CodeStubAssembler::AllocateSlicedString(
    Heap::RootListIndex map_root_index, Node* length, Node* parent,
    Node* offset) {
  CSA_ASSERT(this, TaggedIsSmi(length));
  Node* result = Allocate(SlicedString::kSize);
  Node* map = LoadRoot(map_root_index);
  DCHECK(Heap::RootIsImmortalImmovable(map_root_index));
  StoreMapNoWriteBarrier(result, map);
  StoreObjectFieldNoWriteBarrier(result, SlicedString::kLengthOffset, length,
                                 MachineRepresentation::kTagged);
  StoreObjectFieldNoWriteBarrier(result, SlicedString::kHashFieldOffset,
                                 Int32Constant(String::kEmptyHashField),
                                 MachineRepresentation::kWord32);
  StoreObjectFieldNoWriteBarrier(result, SlicedString::kParentOffset, parent,
                                 MachineRepresentation::kTagged);
  StoreObjectFieldNoWriteBarrier(result, SlicedString::kOffsetOffset, offset,
                                 MachineRepresentation::kTagged);
  return result;
}

Node* CodeStubAssembler::AllocateSlicedOneByteString(Node* length, Node* parent,
                                                     Node* offset) {
  return AllocateSlicedString(Heap::kSlicedOneByteStringMapRootIndex, length,
                              parent, offset);
}

Node* CodeStubAssembler::AllocateSlicedTwoByteString(Node* length, Node* parent,
                                                     Node* offset) {
  return AllocateSlicedString(Heap::kSlicedStringMapRootIndex, length, parent,
                              offset);
}

Node* CodeStubAssembler::AllocateConsString(Heap::RootListIndex map_root_index,
                                            Node* length, Node* first,
                                            Node* second,
                                            AllocationFlags flags) {
  CSA_ASSERT(this, TaggedIsSmi(length));
  Node* result = Allocate(ConsString::kSize, flags);
  Node* map = LoadRoot(map_root_index);
  DCHECK(Heap::RootIsImmortalImmovable(map_root_index));
  StoreMapNoWriteBarrier(result, map);
  StoreObjectFieldNoWriteBarrier(result, ConsString::kLengthOffset, length,
                                 MachineRepresentation::kTagged);
  StoreObjectFieldNoWriteBarrier(result, ConsString::kHashFieldOffset,
                                 Int32Constant(String::kEmptyHashField),
                                 MachineRepresentation::kWord32);
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

Node* CodeStubAssembler::AllocateOneByteConsString(Node* length, Node* first,
                                                   Node* second,
                                                   AllocationFlags flags) {
  return AllocateConsString(Heap::kConsOneByteStringMapRootIndex, length, first,
                            second, flags);
}

Node* CodeStubAssembler::AllocateTwoByteConsString(Node* length, Node* first,
                                                   Node* second,
                                                   AllocationFlags flags) {
  return AllocateConsString(Heap::kConsStringMapRootIndex, length, first,
                            second, flags);
}

Node* CodeStubAssembler::NewConsString(Node* context, Node* length, Node* left,
                                       Node* right, AllocationFlags flags) {
  CSA_ASSERT(this, TaggedIsSmi(length));
  // Added string can be a cons string.
  Comment("Allocating ConsString");
  Node* left_instance_type = LoadInstanceType(left);
  Node* right_instance_type = LoadInstanceType(right);

  // Compute intersection and difference of instance types.
  Node* anded_instance_types = WordAnd(left_instance_type, right_instance_type);
  Node* xored_instance_types = WordXor(left_instance_type, right_instance_type);

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
  Variable result(this, MachineRepresentation::kTagged);
  Label done(this, &result);
  GotoIf(WordNotEqual(
             WordAnd(anded_instance_types,
                     IntPtrConstant(kStringEncodingMask | kOneByteDataHintTag)),
             IntPtrConstant(0)),
         &one_byte_map);
  Branch(WordNotEqual(WordAnd(xored_instance_types,
                              IntPtrConstant(kStringEncodingMask |
                                             kOneByteDataHintMask)),
                      IntPtrConstant(kOneByteStringTag | kOneByteDataHintTag)),
         &two_byte_map, &one_byte_map);

  Bind(&one_byte_map);
  Comment("One-byte ConsString");
  result.Bind(AllocateOneByteConsString(length, left, right, flags));
  Goto(&done);

  Bind(&two_byte_map);
  Comment("Two-byte ConsString");
  result.Bind(AllocateTwoByteConsString(length, left, right, flags));
  Goto(&done);

  Bind(&done);

  return result.value();
}

Node* CodeStubAssembler::AllocateRegExpResult(Node* context, Node* length,
                                              Node* index, Node* input) {
  Node* const max_length =
      SmiConstant(Smi::FromInt(JSArray::kInitialMaxFastElementArray));
  CSA_ASSERT(this, SmiLessThanOrEqual(length, max_length));
  USE(max_length);

  // Allocate the JSRegExpResult.
  // TODO(jgruber): Fold JSArray and FixedArray allocations, then remove
  // unneeded store of elements.
  Node* const result = Allocate(JSRegExpResult::kSize);

  // TODO(jgruber): Store map as Heap constant?
  Node* const native_context = LoadNativeContext(context);
  Node* const map =
      LoadContextElement(native_context, Context::REGEXP_RESULT_MAP_INDEX);
  StoreMapNoWriteBarrier(result, map);

  // Initialize the header before allocating the elements.
  Node* const empty_array = EmptyFixedArrayConstant();
  DCHECK(Heap::RootIsImmortalImmovable(Heap::kEmptyFixedArrayRootIndex));
  StoreObjectFieldNoWriteBarrier(result, JSArray::kPropertiesOffset,
                                 empty_array);
  StoreObjectFieldNoWriteBarrier(result, JSArray::kElementsOffset, empty_array);
  StoreObjectFieldNoWriteBarrier(result, JSArray::kLengthOffset, length);

  StoreObjectFieldNoWriteBarrier(result, JSRegExpResult::kIndexOffset, index);
  StoreObjectField(result, JSRegExpResult::kInputOffset, input);

  Node* const zero = IntPtrConstant(0);
  Node* const length_intptr = SmiUntag(length);
  const ElementsKind elements_kind = FAST_ELEMENTS;
  const ParameterMode parameter_mode = INTPTR_PARAMETERS;

  Node* const elements =
      AllocateFixedArray(elements_kind, length_intptr, parameter_mode);
  StoreObjectField(result, JSArray::kElementsOffset, elements);

  // Fill in the elements with undefined.
  FillFixedArrayWithValue(elements_kind, elements, zero, length_intptr,
                          Heap::kUndefinedValueRootIndex, parameter_mode);

  return result;
}

Node* CodeStubAssembler::AllocateNameDictionary(int at_least_space_for) {
  return AllocateNameDictionary(IntPtrConstant(at_least_space_for));
}

Node* CodeStubAssembler::AllocateNameDictionary(Node* at_least_space_for) {
  CSA_ASSERT(this, UintPtrLessThanOrEqual(
                       at_least_space_for,
                       IntPtrConstant(NameDictionary::kMaxCapacity)));

  Node* capacity = HashTableComputeCapacity(at_least_space_for);
  CSA_ASSERT(this, WordIsPowerOfTwo(capacity));

  Node* length = EntryToIndex<NameDictionary>(capacity);
  Node* store_size =
      IntPtrAddFoldConstants(WordShl(length, IntPtrConstant(kPointerSizeLog2)),
                             IntPtrConstant(NameDictionary::kHeaderSize));

  Node* result = Allocate(store_size);
  Comment("Initialize NameDictionary");
  // Initialize FixedArray fields.
  StoreObjectFieldRoot(result, FixedArray::kMapOffset,
                       Heap::kHashTableMapRootIndex);
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
  Node* filler = LoadRoot(Heap::kUndefinedValueRootIndex);
  StoreFixedArrayElement(result, NameDictionary::kMaxNumberKeyIndex, filler,
                         SKIP_WRITE_BARRIER);
  StoreFixedArrayElement(result, NameDictionary::kNextEnumerationIndexIndex,
                         SmiConstant(PropertyDetails::kInitialIndex),
                         SKIP_WRITE_BARRIER);

  // Initialize NameDictionary elements.
  result = BitcastTaggedToWord(result);
  Node* start_address = IntPtrAdd(
      result, IntPtrConstant(NameDictionary::OffsetOfElementAt(
                                 NameDictionary::kElementsStartIndex) -
                             kHeapObjectTag));
  Node* end_address = IntPtrAdd(
      result,
      IntPtrSubFoldConstants(store_size, IntPtrConstant(kHeapObjectTag)));
  StoreFieldsNoWriteBarrier(start_address, end_address, filler);
  return result;
}

Node* CodeStubAssembler::AllocateJSObjectFromMap(Node* map, Node* properties,
                                                 Node* elements) {
  CSA_ASSERT(this, IsMap(map));
  Node* size =
      IntPtrMul(LoadMapInstanceSize(map), IntPtrConstant(kPointerSize));
  CSA_ASSERT(this, IsRegularHeapObjectSize(size));
  Node* object = Allocate(size);
  StoreMapNoWriteBarrier(object, map);
  InitializeJSObjectFromMap(object, map, size, properties, elements);
  return object;
}

void CodeStubAssembler::InitializeJSObjectFromMap(Node* object, Node* map,
                                                  Node* size, Node* properties,
                                                  Node* elements) {
  // This helper assumes that the object is in new-space, as guarded by the
  // check in AllocatedJSObjectFromMap.
  if (properties == nullptr) {
    CSA_ASSERT(this, Word32BinaryNot(IsDictionaryMap((map))));
    StoreObjectFieldRoot(object, JSObject::kPropertiesOffset,
                         Heap::kEmptyFixedArrayRootIndex);
  } else {
    StoreObjectFieldNoWriteBarrier(object, JSObject::kPropertiesOffset,
                                   properties);
  }
  if (elements == nullptr) {
    StoreObjectFieldRoot(object, JSObject::kElementsOffset,
                         Heap::kEmptyFixedArrayRootIndex);
  } else {
    StoreObjectFieldNoWriteBarrier(object, JSObject::kElementsOffset, elements);
  }
  InitializeJSObjectBody(object, map, size, JSObject::kHeaderSize);
}

void CodeStubAssembler::InitializeJSObjectBody(Node* object, Node* map,
                                               Node* size, int start_offset) {
  // TODO(cbruni): activate in-object slack tracking machinery.
  Comment("InitializeJSObjectBody");
  Node* filler = LoadRoot(Heap::kUndefinedValueRootIndex);
  // Calculate the untagged field addresses.
  Node* start_address =
      IntPtrAdd(object, IntPtrConstant(start_offset - kHeapObjectTag));
  Node* end_address =
      IntPtrSub(IntPtrAdd(object, size), IntPtrConstant(kHeapObjectTag));
  StoreFieldsNoWriteBarrier(start_address, end_address, filler);
}

void CodeStubAssembler::StoreFieldsNoWriteBarrier(Node* start_address,
                                                  Node* end_address,
                                                  Node* value) {
  Comment("StoreFieldsNoWriteBarrier");
  CSA_ASSERT(this, WordIsWordAligned(start_address));
  CSA_ASSERT(this, WordIsWordAligned(end_address));
  BuildFastLoop(
      MachineType::PointerRepresentation(), start_address, end_address,
      [value](CodeStubAssembler* a, Node* current) {
        a->StoreNoWriteBarrier(MachineRepresentation::kTagged, current, value);
      },
      kPointerSize, IndexAdvanceMode::kPost);
}

Node* CodeStubAssembler::AllocateUninitializedJSArrayWithoutElements(
    ElementsKind kind, Node* array_map, Node* length, Node* allocation_site) {
  Comment("begin allocation of JSArray without elements");
  int base_size = JSArray::kSize;
  if (allocation_site != nullptr) {
    base_size += AllocationMemento::kSize;
  }

  Node* size = IntPtrConstant(base_size);
  Node* array = AllocateUninitializedJSArray(kind, array_map, length,
                                             allocation_site, size);
  return array;
}

std::pair<Node*, Node*>
CodeStubAssembler::AllocateUninitializedJSArrayWithElements(
    ElementsKind kind, Node* array_map, Node* length, Node* allocation_site,
    Node* capacity, ParameterMode capacity_mode) {
  Comment("begin allocation of JSArray with elements");
  int base_size = JSArray::kSize;

  if (allocation_site != nullptr) {
    base_size += AllocationMemento::kSize;
  }

  int elements_offset = base_size;

  // Compute space for elements
  base_size += FixedArray::kHeaderSize;
  Node* size = ElementOffsetFromIndex(capacity, kind, capacity_mode, base_size);

  Node* array = AllocateUninitializedJSArray(kind, array_map, length,
                                             allocation_site, size);

  // The bitcast here is safe because InnerAllocate doesn't actually allocate.
  Node* elements = InnerAllocate(BitcastTaggedToWord(array), elements_offset);
  StoreObjectField(array, JSObject::kElementsOffset, elements);

  return {array, elements};
}

Node* CodeStubAssembler::AllocateUninitializedJSArray(ElementsKind kind,
                                                      Node* array_map,
                                                      Node* length,
                                                      Node* allocation_site,
                                                      Node* size_in_bytes) {
  Node* array = Allocate(size_in_bytes);

  Comment("write JSArray headers");
  StoreMapNoWriteBarrier(array, array_map);

  StoreObjectFieldNoWriteBarrier(array, JSArray::kLengthOffset, length);

  StoreObjectFieldRoot(array, JSArray::kPropertiesOffset,
                       Heap::kEmptyFixedArrayRootIndex);

  if (allocation_site != nullptr) {
    InitializeAllocationMemento(array, JSArray::kSize, allocation_site);
  }
  return array;
}

Node* CodeStubAssembler::AllocateJSArray(ElementsKind kind, Node* array_map,
                                         Node* capacity, Node* length,
                                         Node* allocation_site,
                                         ParameterMode capacity_mode) {
  bool is_double = IsFastDoubleElementsKind(kind);

  // Allocate both array and elements object, and initialize the JSArray.
  Node *array, *elements;
  std::tie(array, elements) = AllocateUninitializedJSArrayWithElements(
      kind, array_map, length, allocation_site, capacity, capacity_mode);
  // Setup elements object.
  Heap* heap = isolate()->heap();
  Handle<Map> elements_map(is_double ? heap->fixed_double_array_map()
                                     : heap->fixed_array_map());
  StoreMapNoWriteBarrier(elements, HeapConstant(elements_map));
  StoreObjectFieldNoWriteBarrier(elements, FixedArray::kLengthOffset,
                                 TagParameter(capacity, capacity_mode));

  // Fill in the elements with holes.
  FillFixedArrayWithValue(
      kind, elements, capacity_mode == SMI_PARAMETERS ? SmiConstant(Smi::kZero)
                                                      : IntPtrConstant(0),
      capacity, Heap::kTheHoleValueRootIndex, capacity_mode);

  return array;
}

Node* CodeStubAssembler::AllocateFixedArray(ElementsKind kind,
                                            Node* capacity_node,
                                            ParameterMode mode,
                                            AllocationFlags flags) {
  CSA_ASSERT(this,
             IntPtrGreaterThan(capacity_node, IntPtrOrSmiConstant(0, mode)));
  Node* total_size = GetFixedArrayAllocationSize(capacity_node, kind, mode);

  // Allocate both array and elements object, and initialize the JSArray.
  Node* array = Allocate(total_size, flags);
  Heap* heap = isolate()->heap();
  Handle<Map> map(IsFastDoubleElementsKind(kind)
                      ? heap->fixed_double_array_map()
                      : heap->fixed_array_map());
  if (flags & kPretenured) {
    StoreObjectField(array, JSObject::kMapOffset, HeapConstant(map));
  } else {
    StoreMapNoWriteBarrier(array, HeapConstant(map));
  }
  StoreObjectFieldNoWriteBarrier(array, FixedArray::kLengthOffset,
                                 TagParameter(capacity_node, mode));
  return array;
}

void CodeStubAssembler::FillFixedArrayWithValue(
    ElementsKind kind, Node* array, Node* from_node, Node* to_node,
    Heap::RootListIndex value_root_index, ParameterMode mode) {
  bool is_double = IsFastDoubleElementsKind(kind);
  DCHECK(value_root_index == Heap::kTheHoleValueRootIndex ||
         value_root_index == Heap::kUndefinedValueRootIndex);
  DCHECK_IMPLIES(is_double, value_root_index == Heap::kTheHoleValueRootIndex);
  STATIC_ASSERT(kHoleNanLower32 == kHoleNanUpper32);
  Node* double_hole =
      Is64() ? Int64Constant(kHoleNanInt64) : Int32Constant(kHoleNanLower32);
  Node* value = LoadRoot(value_root_index);

  BuildFastFixedArrayForEach(
      array, kind, from_node, to_node,
      [value, is_double, double_hole](CodeStubAssembler* assembler, Node* array,
                                      Node* offset) {
        if (is_double) {
          // Don't use doubles to store the hole double, since manipulating the
          // signaling NaN used for the hole in C++, e.g. with bit_cast, will
          // change its value on ia32 (the x87 stack is used to return values
          // and stores to the stack silently clear the signalling bit).
          //
          // TODO(danno): When we have a Float32/Float64 wrapper class that
          // preserves double bits during manipulation, remove this code/change
          // this to an indexed Float64 store.
          if (assembler->Is64()) {
            assembler->StoreNoWriteBarrier(MachineRepresentation::kWord64,
                                           array, offset, double_hole);
          } else {
            assembler->StoreNoWriteBarrier(MachineRepresentation::kWord32,
                                           array, offset, double_hole);
            assembler->StoreNoWriteBarrier(
                MachineRepresentation::kWord32, array,
                assembler->IntPtrAdd(offset,
                                     assembler->IntPtrConstant(kPointerSize)),
                double_hole);
          }
        } else {
          assembler->StoreNoWriteBarrier(MachineRepresentation::kTagged, array,
                                         offset, value);
        }
      },
      mode);
}

void CodeStubAssembler::CopyFixedArrayElements(
    ElementsKind from_kind, Node* from_array, ElementsKind to_kind,
    Node* to_array, Node* element_count, Node* capacity,
    WriteBarrierMode barrier_mode, ParameterMode mode) {
  STATIC_ASSERT(FixedArray::kHeaderSize == FixedDoubleArray::kHeaderSize);
  const int first_element_offset = FixedArray::kHeaderSize - kHeapObjectTag;
  Comment("[ CopyFixedArrayElements");

  // Typed array elements are not supported.
  DCHECK(!IsFixedTypedArrayElementsKind(from_kind));
  DCHECK(!IsFixedTypedArrayElementsKind(to_kind));

  Label done(this);
  bool from_double_elements = IsFastDoubleElementsKind(from_kind);
  bool to_double_elements = IsFastDoubleElementsKind(to_kind);
  bool element_size_matches =
      Is64() ||
      IsFastDoubleElementsKind(from_kind) == IsFastDoubleElementsKind(to_kind);
  bool doubles_to_objects_conversion =
      IsFastDoubleElementsKind(from_kind) && IsFastObjectElementsKind(to_kind);
  bool needs_write_barrier =
      doubles_to_objects_conversion || (barrier_mode == UPDATE_WRITE_BARRIER &&
                                        IsFastObjectElementsKind(to_kind));
  Node* double_hole =
      Is64() ? Int64Constant(kHoleNanInt64) : Int32Constant(kHoleNanLower32);

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

  Node* limit_offset = ElementOffsetFromIndex(
      IntPtrOrSmiConstant(0, mode), from_kind, mode, first_element_offset);
  Variable var_from_offset(this, MachineType::PointerRepresentation());
  var_from_offset.Bind(ElementOffsetFromIndex(element_count, from_kind, mode,
                                              first_element_offset));
  // This second variable is used only when the element sizes of source and
  // destination arrays do not match.
  Variable var_to_offset(this, MachineType::PointerRepresentation());
  if (element_size_matches) {
    var_to_offset.Bind(var_from_offset.value());
  } else {
    var_to_offset.Bind(ElementOffsetFromIndex(element_count, to_kind, mode,
                                              first_element_offset));
  }

  Variable* vars[] = {&var_from_offset, &var_to_offset};
  Label decrement(this, 2, vars);

  Branch(WordEqual(var_from_offset.value(), limit_offset), &done, &decrement);

  Bind(&decrement);
  {
    Node* from_offset = IntPtrSub(
        var_from_offset.value(),
        IntPtrConstant(from_double_elements ? kDoubleSize : kPointerSize));
    var_from_offset.Bind(from_offset);

    Node* to_offset;
    if (element_size_matches) {
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
    } else if (IsFastDoubleElementsKind(to_kind)) {
      if_hole = &store_double_hole;
    } else {
      // In all the other cases don't check for holes and copy the data as is.
      if_hole = nullptr;
    }

    Node* value = LoadElementAndPrepareForStore(
        from_array, var_from_offset.value(), from_kind, to_kind, if_hole);

    if (needs_write_barrier) {
      Store(MachineRepresentation::kTagged, to_array, to_offset, value);
    } else if (to_double_elements) {
      StoreNoWriteBarrier(MachineRepresentation::kFloat64, to_array, to_offset,
                          value);
    } else {
      StoreNoWriteBarrier(MachineRepresentation::kTagged, to_array, to_offset,
                          value);
    }
    Goto(&next_iter);

    if (if_hole == &store_double_hole) {
      Bind(&store_double_hole);
      // Don't use doubles to store the hole double, since manipulating the
      // signaling NaN used for the hole in C++, e.g. with bit_cast, will
      // change its value on ia32 (the x87 stack is used to return values
      // and stores to the stack silently clear the signalling bit).
      //
      // TODO(danno): When we have a Float32/Float64 wrapper class that
      // preserves double bits during manipulation, remove this code/change
      // this to an indexed Float64 store.
      if (Is64()) {
        StoreNoWriteBarrier(MachineRepresentation::kWord64, to_array, to_offset,
                            double_hole);
      } else {
        StoreNoWriteBarrier(MachineRepresentation::kWord32, to_array, to_offset,
                            double_hole);
        StoreNoWriteBarrier(MachineRepresentation::kWord32, to_array,
                            IntPtrAdd(to_offset, IntPtrConstant(kPointerSize)),
                            double_hole);
      }
      Goto(&next_iter);
    }

    Bind(&next_iter);
    Node* compare = WordNotEqual(from_offset, limit_offset);
    Branch(compare, &decrement, &done);
  }

  Bind(&done);
  IncrementCounter(isolate()->counters()->inlined_copied_elements(), 1);
  Comment("] CopyFixedArrayElements");
}

void CodeStubAssembler::CopyStringCharacters(
    compiler::Node* from_string, compiler::Node* to_string,
    compiler::Node* from_index, compiler::Node* to_index,
    compiler::Node* character_count, String::Encoding from_encoding,
    String::Encoding to_encoding, ParameterMode mode) {
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
  Node* from_offset =
      ElementOffsetFromIndex(from_index, from_kind, mode, header_size);
  Node* to_offset =
      ElementOffsetFromIndex(to_index, to_kind, mode, header_size);
  Node* byte_count = ElementOffsetFromIndex(character_count, from_kind, mode);
  Node* limit_offset = IntPtrAddFoldConstants(from_offset, byte_count);

  // Prepare the fast loop
  MachineType type =
      from_one_byte ? MachineType::Uint8() : MachineType::Uint16();
  MachineRepresentation rep = to_one_byte ? MachineRepresentation::kWord8
                                          : MachineRepresentation::kWord16;
  int from_increment = 1 << ElementsKindToShiftSize(from_kind);
  int to_increment = 1 << ElementsKindToShiftSize(to_kind);

  Variable current_to_offset(this, MachineType::PointerRepresentation());
  VariableList vars({&current_to_offset}, zone());
  current_to_offset.Bind(to_offset);
  int to_index_constant = 0, from_index_constant = 0;
  Smi* to_index_smi = nullptr;
  Smi* from_index_smi = nullptr;
  bool index_same = (from_encoding == to_encoding) &&
                    (from_index == to_index ||
                     (ToInt32Constant(from_index, from_index_constant) &&
                      ToInt32Constant(to_index, to_index_constant) &&
                      from_index_constant == to_index_constant) ||
                     (ToSmiConstant(from_index, from_index_smi) &&
                      ToSmiConstant(to_index, to_index_smi) &&
                      to_index_smi == from_index_smi));
  BuildFastLoop(vars, MachineType::PointerRepresentation(), from_offset,
                limit_offset,
                [from_string, to_string, &current_to_offset, to_increment, type,
                 rep, index_same](CodeStubAssembler* assembler, Node* offset) {
                  Node* value = assembler->Load(type, from_string, offset);
                  assembler->StoreNoWriteBarrier(
                      rep, to_string,
                      index_same ? offset : current_to_offset.value(), value);
                  if (!index_same) {
                    current_to_offset.Bind(assembler->IntPtrAdd(
                        current_to_offset.value(),
                        assembler->IntPtrConstant(to_increment)));
                  }
                },
                from_increment, IndexAdvanceMode::kPost);
}

Node* CodeStubAssembler::LoadElementAndPrepareForStore(Node* array,
                                                       Node* offset,
                                                       ElementsKind from_kind,
                                                       ElementsKind to_kind,
                                                       Label* if_hole) {
  if (IsFastDoubleElementsKind(from_kind)) {
    Node* value =
        LoadDoubleWithHoleCheck(array, offset, if_hole, MachineType::Float64());
    if (!IsFastDoubleElementsKind(to_kind)) {
      value = AllocateHeapNumberWithValue(value);
    }
    return value;

  } else {
    Node* value = Load(MachineType::AnyTagged(), array, offset);
    if (if_hole) {
      GotoIf(WordEqual(value, TheHoleConstant()), if_hole);
    }
    if (IsFastDoubleElementsKind(to_kind)) {
      if (IsFastSmiElementsKind(from_kind)) {
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
  Node* half_old_capacity = WordShr(old_capacity, IntPtrConstant(1));
  Node* new_capacity = IntPtrAdd(half_old_capacity, old_capacity);
  Node* unconditioned_result =
      IntPtrAdd(new_capacity, IntPtrOrSmiConstant(16, mode));
  if (mode == INTEGER_PARAMETERS || mode == INTPTR_PARAMETERS) {
    return unconditioned_result;
  } else {
    int const kSmiShiftBits = kSmiShiftSize + kSmiTagSize;
    return WordAnd(unconditioned_result,
                   IntPtrConstant(static_cast<size_t>(-1) << kSmiShiftBits));
  }
}

Node* CodeStubAssembler::TryGrowElementsCapacity(Node* object, Node* elements,
                                                 ElementsKind kind, Node* key,
                                                 Label* bailout) {
  Node* capacity = LoadFixedArrayBaseLength(elements);

  ParameterMode mode = OptimalParameterMode();
  capacity = UntagParameter(capacity, mode);
  key = UntagParameter(key, mode);

  return TryGrowElementsCapacity(object, elements, kind, key, capacity, mode,
                                 bailout);
}

Node* CodeStubAssembler::TryGrowElementsCapacity(Node* object, Node* elements,
                                                 ElementsKind kind, Node* key,
                                                 Node* capacity,
                                                 ParameterMode mode,
                                                 Label* bailout) {
  Comment("TryGrowElementsCapacity");

  // If the gap growth is too big, fall back to the runtime.
  Node* max_gap = IntPtrOrSmiConstant(JSObject::kMaxGap, mode);
  Node* max_capacity = IntPtrAdd(capacity, max_gap);
  GotoIf(UintPtrGreaterThanOrEqual(key, max_capacity), bailout);

  // Calculate the capacity of the new backing store.
  Node* new_capacity = CalculateNewElementsCapacity(
      IntPtrAdd(key, IntPtrOrSmiConstant(1, mode)), mode);
  return GrowElementsCapacity(object, elements, kind, kind, capacity,
                              new_capacity, mode, bailout);
}

Node* CodeStubAssembler::GrowElementsCapacity(
    Node* object, Node* elements, ElementsKind from_kind, ElementsKind to_kind,
    Node* capacity, Node* new_capacity, ParameterMode mode, Label* bailout) {
  Comment("[ GrowElementsCapacity");
  // If size of the allocation for the new capacity doesn't fit in a page
  // that we can bump-pointer allocate from, fall back to the runtime.
  int max_size = FixedArrayBase::GetMaxLengthForNewSpaceAllocation(to_kind);
  GotoIf(UintPtrGreaterThanOrEqual(new_capacity,
                                   IntPtrOrSmiConstant(max_size, mode)),
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

void CodeStubAssembler::InitializeAllocationMemento(
    compiler::Node* base_allocation, int base_allocation_size,
    compiler::Node* allocation_site) {
  StoreObjectFieldNoWriteBarrier(
      base_allocation, AllocationMemento::kMapOffset + base_allocation_size,
      HeapConstant(Handle<Map>(isolate()->heap()->allocation_memento_map())));
  StoreObjectFieldNoWriteBarrier(
      base_allocation,
      AllocationMemento::kAllocationSiteOffset + base_allocation_size,
      allocation_site);
  if (FLAG_allocation_site_pretenuring) {
    Node* count = LoadObjectField(allocation_site,
                                  AllocationSite::kPretenureCreateCountOffset);
    Node* incremented_count = SmiAdd(count, SmiConstant(Smi::FromInt(1)));
    StoreObjectFieldNoWriteBarrier(allocation_site,
                                   AllocationSite::kPretenureCreateCountOffset,
                                   incremented_count);
  }
}

Node* CodeStubAssembler::TryTaggedToFloat64(Node* value,
                                            Label* if_valueisnotnumber) {
  Label out(this);
  Variable var_result(this, MachineRepresentation::kFloat64);

  // Check if the {value} is a Smi or a HeapObject.
  Label if_valueissmi(this), if_valueisnotsmi(this);
  Branch(TaggedIsSmi(value), &if_valueissmi, &if_valueisnotsmi);

  Bind(&if_valueissmi);
  {
    // Convert the Smi {value}.
    var_result.Bind(SmiToFloat64(value));
    Goto(&out);
  }

  Bind(&if_valueisnotsmi);
  {
    // Check if {value} is a HeapNumber.
    Label if_valueisheapnumber(this);
    Branch(IsHeapNumberMap(LoadMap(value)), &if_valueisheapnumber,
           if_valueisnotnumber);

    Bind(&if_valueisheapnumber);
    {
      // Load the floating point value.
      var_result.Bind(LoadHeapNumberValue(value));
      Goto(&out);
    }
  }
  Bind(&out);
  return var_result.value();
}

Node* CodeStubAssembler::TruncateTaggedToFloat64(Node* context, Node* value) {
  // We might need to loop once due to ToNumber conversion.
  Variable var_value(this, MachineRepresentation::kTagged),
      var_result(this, MachineRepresentation::kFloat64);
  Label loop(this, &var_value), done_loop(this, &var_result);
  var_value.Bind(value);
  Goto(&loop);
  Bind(&loop);
  {
    Label if_valueisnotnumber(this, Label::kDeferred);

    // Load the current {value}.
    value = var_value.value();

    // Convert {value} to Float64 if it is a number and convert it to a number
    // otherwise.
    Node* const result = TryTaggedToFloat64(value, &if_valueisnotnumber);
    var_result.Bind(result);
    Goto(&done_loop);

    Bind(&if_valueisnotnumber);
    {
      // Convert the {value} to a Number first.
      Callable callable = CodeFactory::NonNumberToNumber(isolate());
      var_value.Bind(CallStub(callable, context, value));
      Goto(&loop);
    }
  }
  Bind(&done_loop);
  return var_result.value();
}

Node* CodeStubAssembler::TruncateTaggedToWord32(Node* context, Node* value) {
  // We might need to loop once due to ToNumber conversion.
  Variable var_value(this, MachineRepresentation::kTagged),
      var_result(this, MachineRepresentation::kWord32);
  Label loop(this, &var_value), done_loop(this, &var_result);
  var_value.Bind(value);
  Goto(&loop);
  Bind(&loop);
  {
    // Load the current {value}.
    value = var_value.value();

    // Check if the {value} is a Smi or a HeapObject.
    Label if_valueissmi(this), if_valueisnotsmi(this);
    Branch(TaggedIsSmi(value), &if_valueissmi, &if_valueisnotsmi);

    Bind(&if_valueissmi);
    {
      // Convert the Smi {value}.
      var_result.Bind(SmiToWord32(value));
      Goto(&done_loop);
    }

    Bind(&if_valueisnotsmi);
    {
      // Check if {value} is a HeapNumber.
      Label if_valueisheapnumber(this),
          if_valueisnotheapnumber(this, Label::kDeferred);
      Branch(WordEqual(LoadMap(value), HeapNumberMapConstant()),
             &if_valueisheapnumber, &if_valueisnotheapnumber);

      Bind(&if_valueisheapnumber);
      {
        // Truncate the floating point value.
        var_result.Bind(TruncateHeapNumberValueToWord32(value));
        Goto(&done_loop);
      }

      Bind(&if_valueisnotheapnumber);
      {
        // Convert the {value} to a Number first.
        Callable callable = CodeFactory::NonNumberToNumber(isolate());
        var_value.Bind(CallStub(callable, context, value));
        Goto(&loop);
      }
    }
  }
  Bind(&done_loop);
  return var_result.value();
}

Node* CodeStubAssembler::TruncateHeapNumberValueToWord32(Node* object) {
  Node* value = LoadHeapNumberValue(object);
  return TruncateFloat64ToWord32(value);
}

Node* CodeStubAssembler::ChangeFloat64ToTagged(Node* value) {
  Node* value32 = RoundFloat64ToInt32(value);
  Node* value64 = ChangeInt32ToFloat64(value32);

  Label if_valueisint32(this), if_valueisheapnumber(this), if_join(this);

  Label if_valueisequal(this), if_valueisnotequal(this);
  Branch(Float64Equal(value, value64), &if_valueisequal, &if_valueisnotequal);
  Bind(&if_valueisequal);
  {
    GotoUnless(Word32Equal(value32, Int32Constant(0)), &if_valueisint32);
    Branch(Int32LessThan(Float64ExtractHighWord32(value), Int32Constant(0)),
           &if_valueisheapnumber, &if_valueisint32);
  }
  Bind(&if_valueisnotequal);
  Goto(&if_valueisheapnumber);

  Variable var_result(this, MachineRepresentation::kTagged);
  Bind(&if_valueisint32);
  {
    if (Is64()) {
      Node* result = SmiTag(ChangeInt32ToInt64(value32));
      var_result.Bind(result);
      Goto(&if_join);
    } else {
      Node* pair = Int32AddWithOverflow(value32, value32);
      Node* overflow = Projection(1, pair);
      Label if_overflow(this, Label::kDeferred), if_notoverflow(this);
      Branch(overflow, &if_overflow, &if_notoverflow);
      Bind(&if_overflow);
      Goto(&if_valueisheapnumber);
      Bind(&if_notoverflow);
      {
        Node* result = Projection(0, pair);
        var_result.Bind(result);
        Goto(&if_join);
      }
    }
  }
  Bind(&if_valueisheapnumber);
  {
    Node* result = AllocateHeapNumberWithValue(value);
    var_result.Bind(result);
    Goto(&if_join);
  }
  Bind(&if_join);
  return var_result.value();
}

Node* CodeStubAssembler::ChangeInt32ToTagged(Node* value) {
  if (Is64()) {
    return SmiTag(ChangeInt32ToInt64(value));
  }
  Variable var_result(this, MachineRepresentation::kTagged);
  Node* pair = Int32AddWithOverflow(value, value);
  Node* overflow = Projection(1, pair);
  Label if_overflow(this, Label::kDeferred), if_notoverflow(this),
      if_join(this);
  Branch(overflow, &if_overflow, &if_notoverflow);
  Bind(&if_overflow);
  {
    Node* value64 = ChangeInt32ToFloat64(value);
    Node* result = AllocateHeapNumberWithValue(value64);
    var_result.Bind(result);
  }
  Goto(&if_join);
  Bind(&if_notoverflow);
  {
    Node* result = Projection(0, pair);
    var_result.Bind(result);
  }
  Goto(&if_join);
  Bind(&if_join);
  return var_result.value();
}

Node* CodeStubAssembler::ChangeUint32ToTagged(Node* value) {
  Label if_overflow(this, Label::kDeferred), if_not_overflow(this),
      if_join(this);
  Variable var_result(this, MachineRepresentation::kTagged);
  // If {value} > 2^31 - 1, we need to store it in a HeapNumber.
  Branch(Uint32LessThan(Int32Constant(Smi::kMaxValue), value), &if_overflow,
         &if_not_overflow);

  Bind(&if_not_overflow);
  {
    if (Is64()) {
      var_result.Bind(SmiTag(ChangeUint32ToUint64(value)));
    } else {
      // If tagging {value} results in an overflow, we need to use a HeapNumber
      // to represent it.
      Node* pair = Int32AddWithOverflow(value, value);
      Node* overflow = Projection(1, pair);
      GotoIf(overflow, &if_overflow);

      Node* result = Projection(0, pair);
      var_result.Bind(result);
    }
  }
  Goto(&if_join);

  Bind(&if_overflow);
  {
    Node* float64_value = ChangeUint32ToFloat64(value);
    var_result.Bind(AllocateHeapNumberWithValue(float64_value));
  }
  Goto(&if_join);

  Bind(&if_join);
  return var_result.value();
}

Node* CodeStubAssembler::ToThisString(Node* context, Node* value,
                                      char const* method_name) {
  Variable var_value(this, MachineRepresentation::kTagged);
  var_value.Bind(value);

  // Check if the {value} is a Smi or a HeapObject.
  Label if_valueissmi(this, Label::kDeferred), if_valueisnotsmi(this),
      if_valueisstring(this);
  Branch(TaggedIsSmi(value), &if_valueissmi, &if_valueisnotsmi);
  Bind(&if_valueisnotsmi);
  {
    // Load the instance type of the {value}.
    Node* value_instance_type = LoadInstanceType(value);

    // Check if the {value} is already String.
    Label if_valueisnotstring(this, Label::kDeferred);
    Branch(IsStringInstanceType(value_instance_type), &if_valueisstring,
           &if_valueisnotstring);
    Bind(&if_valueisnotstring);
    {
      // Check if the {value} is null.
      Label if_valueisnullorundefined(this, Label::kDeferred),
          if_valueisnotnullorundefined(this, Label::kDeferred),
          if_valueisnotnull(this, Label::kDeferred);
      Branch(WordEqual(value, NullConstant()), &if_valueisnullorundefined,
             &if_valueisnotnull);
      Bind(&if_valueisnotnull);
      {
        // Check if the {value} is undefined.
        Branch(WordEqual(value, UndefinedConstant()),
               &if_valueisnullorundefined, &if_valueisnotnullorundefined);
        Bind(&if_valueisnotnullorundefined);
        {
          // Convert the {value} to a String.
          Callable callable = CodeFactory::ToString(isolate());
          var_value.Bind(CallStub(callable, context, value));
          Goto(&if_valueisstring);
        }
      }

      Bind(&if_valueisnullorundefined);
      {
        // The {value} is either null or undefined.
        CallRuntime(Runtime::kThrowCalledOnNullOrUndefined, context,
                    HeapConstant(factory()->NewStringFromAsciiChecked(
                        method_name, TENURED)));
        Goto(&if_valueisstring);  // Never reached.
      }
    }
  }
  Bind(&if_valueissmi);
  {
    // The {value} is a Smi, convert it to a String.
    Callable callable = CodeFactory::NumberToString(isolate());
    var_value.Bind(CallStub(callable, context, value));
    Goto(&if_valueisstring);
  }
  Bind(&if_valueisstring);
  return var_value.value();
}

Node* CodeStubAssembler::ToThisValue(Node* context, Node* value,
                                     PrimitiveType primitive_type,
                                     char const* method_name) {
  // We might need to loop once due to JSValue unboxing.
  Variable var_value(this, MachineRepresentation::kTagged);
  Label loop(this, &var_value), done_loop(this),
      done_throw(this, Label::kDeferred);
  var_value.Bind(value);
  Goto(&loop);
  Bind(&loop);
  {
    // Load the current {value}.
    value = var_value.value();

    // Check if the {value} is a Smi or a HeapObject.
    GotoIf(TaggedIsSmi(value), (primitive_type == PrimitiveType::kNumber)
                                   ? &done_loop
                                   : &done_throw);

    // Load the mape of the {value}.
    Node* value_map = LoadMap(value);

    // Load the instance type of the {value}.
    Node* value_instance_type = LoadMapInstanceType(value_map);

    // Check if {value} is a JSValue.
    Label if_valueisvalue(this, Label::kDeferred), if_valueisnotvalue(this);
    Branch(Word32Equal(value_instance_type, Int32Constant(JS_VALUE_TYPE)),
           &if_valueisvalue, &if_valueisnotvalue);

    Bind(&if_valueisvalue);
    {
      // Load the actual value from the {value}.
      var_value.Bind(LoadObjectField(value, JSValue::kValueOffset));
      Goto(&loop);
    }

    Bind(&if_valueisnotvalue);
    {
      switch (primitive_type) {
        case PrimitiveType::kBoolean:
          GotoIf(WordEqual(value_map, BooleanMapConstant()), &done_loop);
          break;
        case PrimitiveType::kNumber:
          GotoIf(
              Word32Equal(value_instance_type, Int32Constant(HEAP_NUMBER_TYPE)),
              &done_loop);
          break;
        case PrimitiveType::kString:
          GotoIf(IsStringInstanceType(value_instance_type), &done_loop);
          break;
        case PrimitiveType::kSymbol:
          GotoIf(Word32Equal(value_instance_type, Int32Constant(SYMBOL_TYPE)),
                 &done_loop);
          break;
      }
      Goto(&done_throw);
    }
  }

  Bind(&done_throw);
  {
    // The {value} is not a compatible receiver for this method.
    CallRuntime(Runtime::kThrowNotGeneric, context,
                HeapConstant(factory()->NewStringFromAsciiChecked(method_name,
                                                                  TENURED)));
    Goto(&done_loop);  // Never reached.
  }

  Bind(&done_loop);
  return var_value.value();
}

Node* CodeStubAssembler::ThrowIfNotInstanceType(Node* context, Node* value,
                                                InstanceType instance_type,
                                                char const* method_name) {
  Label out(this), throw_exception(this, Label::kDeferred);
  Variable var_value_map(this, MachineRepresentation::kTagged);

  GotoIf(TaggedIsSmi(value), &throw_exception);

  // Load the instance type of the {value}.
  var_value_map.Bind(LoadMap(value));
  Node* const value_instance_type = LoadMapInstanceType(var_value_map.value());

  Branch(Word32Equal(value_instance_type, Int32Constant(instance_type)), &out,
         &throw_exception);

  // The {value} is not a compatible receiver for this method.
  Bind(&throw_exception);
  CallRuntime(
      Runtime::kThrowIncompatibleMethodReceiver, context,
      HeapConstant(factory()->NewStringFromAsciiChecked(method_name, TENURED)),
      value);
  var_value_map.Bind(UndefinedConstant());
  Goto(&out);  // Never reached.

  Bind(&out);
  return var_value_map.value();
}

Node* CodeStubAssembler::IsSpecialReceiverMap(Node* map) {
  Node* is_special = IsSpecialReceiverInstanceType(LoadMapInstanceType(map));
  uint32_t mask =
      1 << Map::kHasNamedInterceptor | 1 << Map::kIsAccessCheckNeeded;
  USE(mask);
  // Interceptors or access checks imply special receiver.
  CSA_ASSERT(this, Select(IsSetWord32(LoadMapBitField(map), mask), is_special,
                          Int32Constant(1), MachineRepresentation::kWord32));
  return is_special;
}

Node* CodeStubAssembler::IsDictionaryMap(Node* map) {
  CSA_SLOW_ASSERT(this, IsMap(map));
  Node* bit_field3 = LoadMapBitField3(map);
  return Word32NotEqual(IsSetWord32<Map::DictionaryMap>(bit_field3),
                        Int32Constant(0));
}

Node* CodeStubAssembler::IsCallableMap(Node* map) {
  CSA_ASSERT(this, IsMap(map));
  return Word32NotEqual(
      Word32And(LoadMapBitField(map), Int32Constant(1 << Map::kIsCallable)),
      Int32Constant(0));
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

Node* CodeStubAssembler::IsJSReceiverInstanceType(Node* instance_type) {
  STATIC_ASSERT(LAST_JS_RECEIVER_TYPE == LAST_TYPE);
  return Int32GreaterThanOrEqual(instance_type,
                                 Int32Constant(FIRST_JS_RECEIVER_TYPE));
}

Node* CodeStubAssembler::IsJSReceiver(Node* object) {
  STATIC_ASSERT(LAST_JS_OBJECT_TYPE == LAST_TYPE);
  return IsJSReceiverInstanceType(LoadInstanceType(object));
}

Node* CodeStubAssembler::IsJSObject(Node* object) {
  STATIC_ASSERT(LAST_JS_OBJECT_TYPE == LAST_TYPE);
  return Int32GreaterThanOrEqual(LoadInstanceType(object),
                                 Int32Constant(FIRST_JS_RECEIVER_TYPE));
}

Node* CodeStubAssembler::IsJSGlobalProxy(Node* object) {
  return Word32Equal(LoadInstanceType(object),
                     Int32Constant(JS_GLOBAL_PROXY_TYPE));
}

Node* CodeStubAssembler::IsMap(Node* map) {
  return HasInstanceType(map, MAP_TYPE);
}

Node* CodeStubAssembler::IsJSValue(Node* map) {
  return HasInstanceType(map, JS_VALUE_TYPE);
}

Node* CodeStubAssembler::IsJSArray(Node* object) {
  return HasInstanceType(object, JS_ARRAY_TYPE);
}

Node* CodeStubAssembler::IsWeakCell(Node* object) {
  return HasInstanceType(object, WEAK_CELL_TYPE);
}

Node* CodeStubAssembler::IsName(Node* object) {
  return Int32LessThanOrEqual(LoadInstanceType(object),
                              Int32Constant(LAST_NAME_TYPE));
}

Node* CodeStubAssembler::IsString(Node* object) {
  return Int32LessThanOrEqual(LoadInstanceType(object),
                              Int32Constant(FIRST_NONSTRING_TYPE));
}

Node* CodeStubAssembler::IsNativeContext(Node* object) {
  return WordEqual(LoadMap(object), LoadRoot(Heap::kNativeContextMapRootIndex));
}

Node* CodeStubAssembler::IsFixedDoubleArray(Node* object) {
  return WordEqual(LoadMap(object), FixedDoubleArrayMapConstant());
}

Node* CodeStubAssembler::IsHashTable(Node* object) {
  return WordEqual(LoadMap(object), LoadRoot(Heap::kHashTableMapRootIndex));
}

Node* CodeStubAssembler::IsDictionary(Node* object) {
  return WordOr(IsHashTable(object), IsUnseededNumberDictionary(object));
}

Node* CodeStubAssembler::IsUnseededNumberDictionary(Node* object) {
  return WordEqual(LoadMap(object),
                   LoadRoot(Heap::kUnseededNumberDictionaryMapRootIndex));
}

Node* CodeStubAssembler::StringCharCodeAt(Node* string, Node* index) {
  CSA_ASSERT(this, IsString(string));
  // Translate the {index} into a Word.
  index = SmiToWord(index);

  // We may need to loop in case of cons or sliced strings.
  Variable var_index(this, MachineType::PointerRepresentation());
  Variable var_result(this, MachineRepresentation::kWord32);
  Variable var_string(this, MachineRepresentation::kTagged);
  Variable* loop_vars[] = {&var_index, &var_string};
  Label done_loop(this, &var_result), loop(this, 2, loop_vars);
  var_string.Bind(string);
  var_index.Bind(index);
  Goto(&loop);
  Bind(&loop);
  {
    // Load the current {index}.
    index = var_index.value();

    // Load the current {string}.
    string = var_string.value();

    // Load the instance type of the {string}.
    Node* string_instance_type = LoadInstanceType(string);

    // Check if the {string} is a SeqString.
    Label if_stringissequential(this), if_stringisnotsequential(this);
    Branch(Word32Equal(Word32And(string_instance_type,
                                 Int32Constant(kStringRepresentationMask)),
                       Int32Constant(kSeqStringTag)),
           &if_stringissequential, &if_stringisnotsequential);

    Bind(&if_stringissequential);
    {
      // Check if the {string} is a TwoByteSeqString or a OneByteSeqString.
      Label if_stringistwobyte(this), if_stringisonebyte(this);
      Branch(Word32Equal(Word32And(string_instance_type,
                                   Int32Constant(kStringEncodingMask)),
                         Int32Constant(kTwoByteStringTag)),
             &if_stringistwobyte, &if_stringisonebyte);

      Bind(&if_stringisonebyte);
      {
        var_result.Bind(
            Load(MachineType::Uint8(), string,
                 IntPtrAdd(index, IntPtrConstant(SeqOneByteString::kHeaderSize -
                                                 kHeapObjectTag))));
        Goto(&done_loop);
      }

      Bind(&if_stringistwobyte);
      {
        var_result.Bind(
            Load(MachineType::Uint16(), string,
                 IntPtrAdd(WordShl(index, IntPtrConstant(1)),
                           IntPtrConstant(SeqTwoByteString::kHeaderSize -
                                          kHeapObjectTag))));
        Goto(&done_loop);
      }
    }

    Bind(&if_stringisnotsequential);
    {
      // Check if the {string} is a ConsString.
      Label if_stringiscons(this), if_stringisnotcons(this);
      Branch(Word32Equal(Word32And(string_instance_type,
                                   Int32Constant(kStringRepresentationMask)),
                         Int32Constant(kConsStringTag)),
             &if_stringiscons, &if_stringisnotcons);

      Bind(&if_stringiscons);
      {
        // Check whether the right hand side is the empty string (i.e. if
        // this is really a flat string in a cons string). If that is not
        // the case we flatten the string first.
        Label if_rhsisempty(this), if_rhsisnotempty(this, Label::kDeferred);
        Node* rhs = LoadObjectField(string, ConsString::kSecondOffset);
        Branch(WordEqual(rhs, EmptyStringConstant()), &if_rhsisempty,
               &if_rhsisnotempty);

        Bind(&if_rhsisempty);
        {
          // Just operate on the left hand side of the {string}.
          var_string.Bind(LoadObjectField(string, ConsString::kFirstOffset));
          Goto(&loop);
        }

        Bind(&if_rhsisnotempty);
        {
          // Flatten the {string} and lookup in the resulting string.
          var_string.Bind(CallRuntime(Runtime::kFlattenString,
                                      NoContextConstant(), string));
          Goto(&loop);
        }
      }

      Bind(&if_stringisnotcons);
      {
        // Check if the {string} is an ExternalString.
        Label if_stringisexternal(this), if_stringisnotexternal(this);
        Branch(Word32Equal(Word32And(string_instance_type,
                                     Int32Constant(kStringRepresentationMask)),
                           Int32Constant(kExternalStringTag)),
               &if_stringisexternal, &if_stringisnotexternal);

        Bind(&if_stringisexternal);
        {
          // Check if the {string} is a short external string.
          Label if_stringisnotshort(this),
              if_stringisshort(this, Label::kDeferred);
          Branch(Word32Equal(Word32And(string_instance_type,
                                       Int32Constant(kShortExternalStringMask)),
                             Int32Constant(0)),
                 &if_stringisnotshort, &if_stringisshort);

          Bind(&if_stringisnotshort);
          {
            // Load the actual resource data from the {string}.
            Node* string_resource_data =
                LoadObjectField(string, ExternalString::kResourceDataOffset,
                                MachineType::Pointer());

            // Check if the {string} is a TwoByteExternalString or a
            // OneByteExternalString.
            Label if_stringistwobyte(this), if_stringisonebyte(this);
            Branch(Word32Equal(Word32And(string_instance_type,
                                         Int32Constant(kStringEncodingMask)),
                               Int32Constant(kTwoByteStringTag)),
                   &if_stringistwobyte, &if_stringisonebyte);

            Bind(&if_stringisonebyte);
            {
              var_result.Bind(
                  Load(MachineType::Uint8(), string_resource_data, index));
              Goto(&done_loop);
            }

            Bind(&if_stringistwobyte);
            {
              var_result.Bind(Load(MachineType::Uint16(), string_resource_data,
                                   WordShl(index, IntPtrConstant(1))));
              Goto(&done_loop);
            }
          }

          Bind(&if_stringisshort);
          {
            // The {string} might be compressed, call the runtime.
            var_result.Bind(SmiToWord32(
                CallRuntime(Runtime::kExternalStringGetChar,
                            NoContextConstant(), string, SmiTag(index))));
            Goto(&done_loop);
          }
        }

        Bind(&if_stringisnotexternal);
        {
          // The {string} is a SlicedString, continue with its parent.
          Node* string_offset =
              LoadAndUntagObjectField(string, SlicedString::kOffsetOffset);
          Node* string_parent =
              LoadObjectField(string, SlicedString::kParentOffset);
          var_index.Bind(IntPtrAdd(index, string_offset));
          var_string.Bind(string_parent);
          Goto(&loop);
        }
      }
    }
  }

  Bind(&done_loop);
  return var_result.value();
}

Node* CodeStubAssembler::StringFromCharCode(Node* code) {
  Variable var_result(this, MachineRepresentation::kTagged);

  // Check if the {code} is a one-byte char code.
  Label if_codeisonebyte(this), if_codeistwobyte(this, Label::kDeferred),
      if_done(this);
  Branch(Int32LessThanOrEqual(code, Int32Constant(String::kMaxOneByteCharCode)),
         &if_codeisonebyte, &if_codeistwobyte);
  Bind(&if_codeisonebyte);
  {
    // Load the isolate wide single character string cache.
    Node* cache = LoadRoot(Heap::kSingleCharacterStringCacheRootIndex);

    // Check if we have an entry for the {code} in the single character string
    // cache already.
    Label if_entryisundefined(this, Label::kDeferred),
        if_entryisnotundefined(this);
    Node* entry = LoadFixedArrayElement(cache, code);
    Branch(WordEqual(entry, UndefinedConstant()), &if_entryisundefined,
           &if_entryisnotundefined);

    Bind(&if_entryisundefined);
    {
      // Allocate a new SeqOneByteString for {code} and store it in the {cache}.
      Node* result = AllocateSeqOneByteString(1);
      StoreNoWriteBarrier(
          MachineRepresentation::kWord8, result,
          IntPtrConstant(SeqOneByteString::kHeaderSize - kHeapObjectTag), code);
      StoreFixedArrayElement(cache, code, result);
      var_result.Bind(result);
      Goto(&if_done);
    }

    Bind(&if_entryisnotundefined);
    {
      // Return the entry from the {cache}.
      var_result.Bind(entry);
      Goto(&if_done);
    }
  }

  Bind(&if_codeistwobyte);
  {
    // Allocate a new SeqTwoByteString for {code}.
    Node* result = AllocateSeqTwoByteString(1);
    StoreNoWriteBarrier(
        MachineRepresentation::kWord16, result,
        IntPtrConstant(SeqTwoByteString::kHeaderSize - kHeapObjectTag), code);
    var_result.Bind(result);
    Goto(&if_done);
  }

  Bind(&if_done);
  return var_result.value();
}

namespace {

// A wrapper around CopyStringCharacters which determines the correct string
// encoding, allocates a corresponding sequential string, and then copies the
// given character range using CopyStringCharacters.
// |from_string| must be a sequential string. |from_index| and
// |character_count| must be Smis s.t.
// 0 <= |from_index| <= |from_index| + |character_count| < from_string.length.
Node* AllocAndCopyStringCharacters(CodeStubAssembler* a, Node* context,
                                   Node* from, Node* from_instance_type,
                                   Node* from_index, Node* character_count) {
  typedef CodeStubAssembler::Label Label;
  typedef CodeStubAssembler::Variable Variable;

  Label end(a), two_byte_sequential(a);
  Variable var_result(a, MachineRepresentation::kTagged);

  Node* const smi_zero = a->SmiConstant(Smi::kZero);

  STATIC_ASSERT((kOneByteStringTag & kStringEncodingMask) != 0);
  a->GotoIf(a->Word32Equal(a->Word32And(from_instance_type,
                                        a->Int32Constant(kStringEncodingMask)),
                           a->Int32Constant(0)),
            &two_byte_sequential);

  // The subject string is a sequential one-byte string.
  {
    Node* result =
        a->AllocateSeqOneByteString(context, a->SmiToWord(character_count));
    a->CopyStringCharacters(from, result, from_index, smi_zero, character_count,
                            String::ONE_BYTE_ENCODING,
                            String::ONE_BYTE_ENCODING,
                            CodeStubAssembler::SMI_PARAMETERS);
    var_result.Bind(result);

    a->Goto(&end);
  }

  // The subject string is a sequential two-byte string.
  a->Bind(&two_byte_sequential);
  {
    Node* result =
        a->AllocateSeqTwoByteString(context, a->SmiToWord(character_count));
    a->CopyStringCharacters(from, result, from_index, smi_zero, character_count,
                            String::TWO_BYTE_ENCODING,
                            String::TWO_BYTE_ENCODING,
                            CodeStubAssembler::SMI_PARAMETERS);
    var_result.Bind(result);

    a->Goto(&end);
  }

  a->Bind(&end);
  return var_result.value();
}

}  // namespace

Node* CodeStubAssembler::SubString(Node* context, Node* string, Node* from,
                                   Node* to) {
  Label end(this);
  Label runtime(this);

  Variable var_instance_type(this, MachineRepresentation::kWord8);  // Int32.
  Variable var_result(this, MachineRepresentation::kTagged);        // String.
  Variable var_from(this, MachineRepresentation::kTagged);          // Smi.
  Variable var_string(this, MachineRepresentation::kTagged);        // String.

  var_instance_type.Bind(Int32Constant(0));
  var_string.Bind(string);
  var_from.Bind(from);

  // Make sure first argument is a string.

  // Bailout if receiver is a Smi.
  GotoIf(TaggedIsSmi(string), &runtime);

  // Load the instance type of the {string}.
  Node* const instance_type = LoadInstanceType(string);
  var_instance_type.Bind(instance_type);

  // Check if {string} is a String.
  GotoUnless(IsStringInstanceType(instance_type), &runtime);

  // Make sure that both from and to are non-negative smis.

  GotoUnless(WordIsPositiveSmi(from), &runtime);
  GotoUnless(WordIsPositiveSmi(to), &runtime);

  Node* const substr_length = SmiSub(to, from);
  Node* const string_length = LoadStringLength(string);

  // Begin dispatching based on substring length.

  Label original_string_or_invalid_length(this);
  GotoIf(SmiAboveOrEqual(substr_length, string_length),
         &original_string_or_invalid_length);

  // A real substring (substr_length < string_length).

  Label single_char(this);
  GotoIf(SmiEqual(substr_length, SmiConstant(Smi::FromInt(1))), &single_char);

  // TODO(jgruber): Add an additional case for substring of length == 0?

  // Deal with different string types: update the index if necessary
  // and put the underlying string into var_string.

  // If the string is not indirect, it can only be sequential or external.
  STATIC_ASSERT(kIsIndirectStringMask == (kSlicedStringTag & kConsStringTag));
  STATIC_ASSERT(kIsIndirectStringMask != 0);
  Label underlying_unpacked(this);
  GotoIf(Word32Equal(
             Word32And(instance_type, Int32Constant(kIsIndirectStringMask)),
             Int32Constant(0)),
         &underlying_unpacked);

  // The subject string is either a sliced or cons string.

  Label sliced_string(this);
  GotoIf(Word32NotEqual(
             Word32And(instance_type, Int32Constant(kSlicedNotConsMask)),
             Int32Constant(0)),
         &sliced_string);

  // Cons string.  Check whether it is flat, then fetch first part.
  // Flat cons strings have an empty second part.
  {
    GotoIf(WordNotEqual(LoadObjectField(string, ConsString::kSecondOffset),
                        EmptyStringConstant()),
           &runtime);

    Node* first_string_part = LoadObjectField(string, ConsString::kFirstOffset);
    var_string.Bind(first_string_part);
    var_instance_type.Bind(LoadInstanceType(first_string_part));

    Goto(&underlying_unpacked);
  }

  Bind(&sliced_string);
  {
    // Fetch parent and correct start index by offset.
    Node* sliced_offset = LoadObjectField(string, SlicedString::kOffsetOffset);
    var_from.Bind(SmiAdd(from, sliced_offset));

    Node* slice_parent = LoadObjectField(string, SlicedString::kParentOffset);
    var_string.Bind(slice_parent);

    Node* slice_parent_instance_type = LoadInstanceType(slice_parent);
    var_instance_type.Bind(slice_parent_instance_type);

    Goto(&underlying_unpacked);
  }

  // The subject string can only be external or sequential string of either
  // encoding at this point.
  Label external_string(this);
  Bind(&underlying_unpacked);
  {
    if (FLAG_string_slices) {
      Label copy_routine(this);

      // Short slice.  Copy instead of slicing.
      GotoIf(SmiLessThan(substr_length,
                         SmiConstant(Smi::FromInt(SlicedString::kMinLength))),
             &copy_routine);

      // Allocate new sliced string.

      Label two_byte_slice(this);
      STATIC_ASSERT((kStringEncodingMask & kOneByteStringTag) != 0);
      STATIC_ASSERT((kStringEncodingMask & kTwoByteStringTag) == 0);

      Counters* counters = isolate()->counters();
      IncrementCounter(counters->sub_string_native(), 1);

      GotoIf(Word32Equal(Word32And(var_instance_type.value(),
                                   Int32Constant(kStringEncodingMask)),
                         Int32Constant(0)),
             &two_byte_slice);

      var_result.Bind(AllocateSlicedOneByteString(
          substr_length, var_string.value(), var_from.value()));
      Goto(&end);

      Bind(&two_byte_slice);

      var_result.Bind(AllocateSlicedTwoByteString(
          substr_length, var_string.value(), var_from.value()));
      Goto(&end);

      Bind(&copy_routine);
    }

    // The subject string can only be external or sequential string of either
    // encoding at this point.
    STATIC_ASSERT(kExternalStringTag != 0);
    STATIC_ASSERT(kSeqStringTag == 0);
    GotoUnless(Word32Equal(Word32And(var_instance_type.value(),
                                     Int32Constant(kExternalStringTag)),
                           Int32Constant(0)),
               &external_string);

    var_result.Bind(AllocAndCopyStringCharacters(
        this, context, var_string.value(), var_instance_type.value(),
        var_from.value(), substr_length));

    Counters* counters = isolate()->counters();
    IncrementCounter(counters->sub_string_native(), 1);

    Goto(&end);
  }

  // Handle external string.
  Bind(&external_string);
  {
    // Rule out short external strings.
    STATIC_ASSERT(kShortExternalStringTag != 0);
    GotoIf(Word32NotEqual(Word32And(var_instance_type.value(),
                                    Int32Constant(kShortExternalStringMask)),
                          Int32Constant(0)),
           &runtime);

    // Move the pointer so that offset-wise, it looks like a sequential string.
    STATIC_ASSERT(SeqTwoByteString::kHeaderSize ==
                  SeqOneByteString::kHeaderSize);

    Node* resource_data = LoadObjectField(var_string.value(),
                                          ExternalString::kResourceDataOffset);
    Node* const fake_sequential_string = IntPtrSub(
        resource_data,
        IntPtrConstant(SeqTwoByteString::kHeaderSize - kHeapObjectTag));

    var_result.Bind(AllocAndCopyStringCharacters(
        this, context, fake_sequential_string, var_instance_type.value(),
        var_from.value(), substr_length));

    Counters* counters = isolate()->counters();
    IncrementCounter(counters->sub_string_native(), 1);

    Goto(&end);
  }

  // Substrings of length 1 are generated through CharCodeAt and FromCharCode.
  Bind(&single_char);
  {
    Node* char_code = StringCharCodeAt(var_string.value(), var_from.value());
    var_result.Bind(StringFromCharCode(char_code));
    Goto(&end);
  }

  Bind(&original_string_or_invalid_length);
  {
    // Longer than original string's length or negative: unsafe arguments.
    GotoIf(SmiAbove(substr_length, string_length), &runtime);

    // Equal length - check if {from, to} == {0, str.length}.
    GotoIf(SmiAbove(from, SmiConstant(Smi::kZero)), &runtime);

    // Return the original string (substr_length == string_length).

    Counters* counters = isolate()->counters();
    IncrementCounter(counters->sub_string_native(), 1);

    var_result.Bind(string);
    Goto(&end);
  }

  // Fall back to a runtime call.
  Bind(&runtime);
  {
    var_result.Bind(
        CallRuntime(Runtime::kSubString, context, string, from, to));
    Goto(&end);
  }

  Bind(&end);
  return var_result.value();
}

Node* CodeStubAssembler::StringAdd(Node* context, Node* left, Node* right,
                                   AllocationFlags flags) {
  Label check_right(this);
  Label runtime(this, Label::kDeferred);
  Label cons(this);
  Label non_cons(this);
  Variable result(this, MachineRepresentation::kTagged);
  Label done(this, &result);
  Label done_native(this, &result);
  Counters* counters = isolate()->counters();

  Node* left_length = LoadStringLength(left);
  GotoIf(WordNotEqual(IntPtrConstant(0), left_length), &check_right);
  result.Bind(right);
  Goto(&done_native);

  Bind(&check_right);
  Node* right_length = LoadStringLength(right);
  GotoIf(WordNotEqual(IntPtrConstant(0), right_length), &cons);
  result.Bind(left);
  Goto(&done_native);

  Bind(&cons);
  CSA_ASSERT(this, TaggedIsSmi(left_length));
  CSA_ASSERT(this, TaggedIsSmi(right_length));
  Node* new_length = SmiAdd(left_length, right_length);
  GotoIf(UintPtrGreaterThanOrEqual(
             new_length, SmiConstant(Smi::FromInt(String::kMaxLength))),
         &runtime);

  GotoIf(IntPtrLessThan(new_length,
                        SmiConstant(Smi::FromInt(ConsString::kMinLength))),
         &non_cons);

  result.Bind(NewConsString(context, new_length, left, right, flags));
  Goto(&done_native);

  Bind(&non_cons);

  Comment("Full string concatenate");
  Node* left_instance_type = LoadInstanceType(left);
  Node* right_instance_type = LoadInstanceType(right);
  // Compute intersection and difference of instance types.

  Node* ored_instance_types = WordOr(left_instance_type, right_instance_type);
  Node* xored_instance_types = WordXor(left_instance_type, right_instance_type);

  // Check if both strings have the same encoding and both are sequential.
  GotoIf(WordNotEqual(
             WordAnd(xored_instance_types, IntPtrConstant(kStringEncodingMask)),
             IntPtrConstant(0)),
         &runtime);
  GotoIf(WordNotEqual(WordAnd(ored_instance_types,
                              IntPtrConstant(kStringRepresentationMask)),
                      IntPtrConstant(0)),
         &runtime);

  Label two_byte(this);
  GotoIf(WordEqual(
             WordAnd(ored_instance_types, IntPtrConstant(kStringEncodingMask)),
             IntPtrConstant(kTwoByteStringTag)),
         &two_byte);
  // One-byte sequential string case
  Node* new_string =
      AllocateSeqOneByteString(context, new_length, SMI_PARAMETERS);
  CopyStringCharacters(left, new_string, SmiConstant(Smi::kZero),
                       SmiConstant(Smi::kZero), left_length,
                       String::ONE_BYTE_ENCODING, String::ONE_BYTE_ENCODING,
                       SMI_PARAMETERS);
  CopyStringCharacters(right, new_string, SmiConstant(Smi::kZero), left_length,
                       right_length, String::ONE_BYTE_ENCODING,
                       String::ONE_BYTE_ENCODING, SMI_PARAMETERS);
  result.Bind(new_string);
  Goto(&done_native);

  Bind(&two_byte);
  {
    // Two-byte sequential string case
    new_string = AllocateSeqTwoByteString(context, new_length, SMI_PARAMETERS);
    CopyStringCharacters(left, new_string, SmiConstant(Smi::kZero),
                         SmiConstant(Smi::kZero), left_length,
                         String::TWO_BYTE_ENCODING, String::TWO_BYTE_ENCODING,
                         SMI_PARAMETERS);
    CopyStringCharacters(right, new_string, SmiConstant(Smi::kZero),
                         left_length, right_length, String::TWO_BYTE_ENCODING,
                         String::TWO_BYTE_ENCODING, SMI_PARAMETERS);
    result.Bind(new_string);
    Goto(&done_native);
  }

  Bind(&runtime);
  {
    result.Bind(CallRuntime(Runtime::kStringAdd, context, left, right));
    Goto(&done);
  }

  Bind(&done_native);
  {
    IncrementCounter(counters->string_add_native(), 1);
    Goto(&done);
  }

  Bind(&done);
  return result.value();
}

Node* CodeStubAssembler::StringIndexOfChar(Node* context, Node* string,
                                           Node* needle_char, Node* from) {
  CSA_ASSERT(this, IsString(string));
  Variable var_result(this, MachineRepresentation::kTagged);

  Label out(this), runtime(this, Label::kDeferred);

  // Let runtime handle non-one-byte {needle_char}.

  Node* const one_byte_char_mask = IntPtrConstant(0xFF);
  GotoUnless(WordEqual(WordAnd(needle_char, one_byte_char_mask), needle_char),
             &runtime);

  // TODO(jgruber): Handle external and two-byte strings.

  Node* const one_byte_seq_mask = Int32Constant(
      kIsIndirectStringMask | kExternalStringTag | kStringEncodingMask);
  Node* const expected_masked = Int32Constant(kOneByteStringTag);

  Node* const string_instance_type = LoadInstanceType(string);
  GotoUnless(Word32Equal(Word32And(string_instance_type, one_byte_seq_mask),
                         expected_masked),
             &runtime);

  // If we reach this, {string} is a non-indirect, non-external one-byte string.

  Node* const length = LoadStringLength(string);
  Node* const search_range_length = SmiUntag(SmiSub(length, from));

  const int offset = SeqOneByteString::kHeaderSize - kHeapObjectTag;
  Node* const begin = IntPtrConstant(offset);
  Node* const cursor = IntPtrAdd(begin, SmiUntag(from));
  Node* const end = IntPtrAdd(cursor, search_range_length);

  var_result.Bind(SmiConstant(Smi::FromInt(-1)));

  BuildFastLoop(MachineType::PointerRepresentation(), cursor, end,
                [string, needle_char, begin, &var_result, &out](
                    CodeStubAssembler* csa, Node* cursor) {
                  Label next(csa);
                  Node* value = csa->Load(MachineType::Uint8(), string, cursor);
                  csa->GotoUnless(csa->WordEqual(value, needle_char), &next);

                  // Found a match.
                  Node* index = csa->SmiTag(csa->IntPtrSub(cursor, begin));
                  var_result.Bind(index);
                  csa->Goto(&out);

                  csa->Bind(&next);
                },
                1, IndexAdvanceMode::kPost);
  Goto(&out);

  Bind(&runtime);
  {
    Node* const pattern = StringFromCharCode(needle_char);
    Node* const result =
        CallRuntime(Runtime::kStringIndexOf, context, string, pattern, from);
    var_result.Bind(result);
    Goto(&out);
  }

  Bind(&out);
  return var_result.value();
}

Node* CodeStubAssembler::StringFromCodePoint(compiler::Node* codepoint,
                                             UnicodeEncoding encoding) {
  Variable var_result(this, MachineRepresentation::kTagged);
  var_result.Bind(EmptyStringConstant());

  Label if_isword16(this), if_isword32(this), return_result(this);

  Branch(Uint32LessThan(codepoint, Int32Constant(0x10000)), &if_isword16,
         &if_isword32);

  Bind(&if_isword16);
  {
    var_result.Bind(StringFromCharCode(codepoint));
    Goto(&return_result);
  }

  Bind(&if_isword32);
  {
    switch (encoding) {
      case UnicodeEncoding::UTF16:
        break;
      case UnicodeEncoding::UTF32: {
        // Convert UTF32 to UTF16 code units, and store as a 32 bit word.
        Node* lead_offset = Int32Constant(0xD800 - (0x10000 >> 10));

        // lead = (codepoint >> 10) + LEAD_OFFSET
        Node* lead =
            Int32Add(WordShr(codepoint, Int32Constant(10)), lead_offset);

        // trail = (codepoint & 0x3FF) + 0xDC00;
        Node* trail = Int32Add(Word32And(codepoint, Int32Constant(0x3FF)),
                               Int32Constant(0xDC00));

        // codpoint = (trail << 16) | lead;
        codepoint = Word32Or(WordShl(trail, Int32Constant(16)), lead);
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

  Bind(&return_result);
  return var_result.value();
}

Node* CodeStubAssembler::StringToNumber(Node* context, Node* input) {
  Label runtime(this, Label::kDeferred);
  Label end(this);

  Variable var_result(this, MachineRepresentation::kTagged);

  // Check if string has a cached array index.
  Node* hash = LoadNameHashField(input);
  Node* bit =
      Word32And(hash, Int32Constant(String::kContainsCachedArrayIndexMask));
  GotoIf(Word32NotEqual(bit, Int32Constant(0)), &runtime);

  var_result.Bind(
      SmiTag(DecodeWordFromWord32<String::ArrayIndexValueBits>(hash)));
  Goto(&end);

  Bind(&runtime);
  {
    var_result.Bind(CallRuntime(Runtime::kStringToNumber, context, input));
    Goto(&end);
  }

  Bind(&end);
  return var_result.value();
}

Node* CodeStubAssembler::NumberToString(compiler::Node* context,
                                        compiler::Node* argument) {
  Variable result(this, MachineRepresentation::kTagged);
  Label runtime(this, Label::kDeferred);
  Label smi(this);
  Label done(this, &result);

  // Load the number string cache.
  Node* number_string_cache = LoadRoot(Heap::kNumberStringCacheRootIndex);

  // Make the hash mask from the length of the number string cache. It
  // contains two elements (number and string) for each cache entry.
  Node* mask = LoadFixedArrayBaseLength(number_string_cache);
  Node* one = IntPtrConstant(1);
  mask = IntPtrSub(mask, one);

  GotoIf(TaggedIsSmi(argument), &smi);

  // Argument isn't smi, check to see if it's a heap-number.
  Node* map = LoadMap(argument);
  GotoUnless(WordEqual(map, HeapNumberMapConstant()), &runtime);

  // Make a hash from the two 32-bit values of the double.
  Node* low =
      LoadObjectField(argument, HeapNumber::kValueOffset, MachineType::Int32());
  Node* high = LoadObjectField(argument, HeapNumber::kValueOffset + kIntSize,
                               MachineType::Int32());
  Node* hash = Word32Xor(low, high);
  if (Is64()) hash = ChangeInt32ToInt64(hash);
  hash = WordShl(hash, one);
  Node* index = WordAnd(hash, SmiToWord(mask));

  // Cache entry's key must be a heap number
  Node* number_key =
      LoadFixedArrayElement(number_string_cache, index, 0, INTPTR_PARAMETERS);
  GotoIf(TaggedIsSmi(number_key), &runtime);
  map = LoadMap(number_key);
  GotoUnless(WordEqual(map, HeapNumberMapConstant()), &runtime);

  // Cache entry's key must match the heap number value we're looking for.
  Node* low_compare = LoadObjectField(number_key, HeapNumber::kValueOffset,
                                      MachineType::Int32());
  Node* high_compare = LoadObjectField(
      number_key, HeapNumber::kValueOffset + kIntSize, MachineType::Int32());
  GotoUnless(WordEqual(low, low_compare), &runtime);
  GotoUnless(WordEqual(high, high_compare), &runtime);

  // Heap number match, return value fro cache entry.
  IncrementCounter(isolate()->counters()->number_to_string_native(), 1);
  result.Bind(LoadFixedArrayElement(number_string_cache, index, kPointerSize,
                                    INTPTR_PARAMETERS));
  Goto(&done);

  Bind(&runtime);
  {
    // No cache entry, go to the runtime.
    result.Bind(CallRuntime(Runtime::kNumberToString, context, argument));
  }
  Goto(&done);

  Bind(&smi);
  {
    // Load the smi key, make sure it matches the smi we're looking for.
    Node* smi_index = WordAnd(WordShl(argument, one), mask);
    Node* smi_key = LoadFixedArrayElement(number_string_cache, smi_index, 0,
                                          SMI_PARAMETERS);
    GotoIf(WordNotEqual(smi_key, argument), &runtime);

    // Smi match, return value from cache entry.
    IncrementCounter(isolate()->counters()->number_to_string_native(), 1);
    result.Bind(LoadFixedArrayElement(number_string_cache, smi_index,
                                      kPointerSize, SMI_PARAMETERS));
    Goto(&done);
  }

  Bind(&done);
  return result.value();
}

Node* CodeStubAssembler::ToName(Node* context, Node* value) {
  typedef CodeStubAssembler::Label Label;
  typedef CodeStubAssembler::Variable Variable;

  Label end(this);
  Variable var_result(this, MachineRepresentation::kTagged);

  Label is_number(this);
  GotoIf(TaggedIsSmi(value), &is_number);

  Label not_name(this);
  Node* value_instance_type = LoadInstanceType(value);
  STATIC_ASSERT(FIRST_NAME_TYPE == FIRST_TYPE);
  GotoIf(Int32GreaterThan(value_instance_type, Int32Constant(LAST_NAME_TYPE)),
         &not_name);

  var_result.Bind(value);
  Goto(&end);

  Bind(&is_number);
  {
    Callable callable = CodeFactory::NumberToString(isolate());
    var_result.Bind(CallStub(callable, context, value));
    Goto(&end);
  }

  Bind(&not_name);
  {
    GotoIf(Word32Equal(value_instance_type, Int32Constant(HEAP_NUMBER_TYPE)),
           &is_number);

    Label not_oddball(this);
    GotoIf(Word32NotEqual(value_instance_type, Int32Constant(ODDBALL_TYPE)),
           &not_oddball);

    var_result.Bind(LoadObjectField(value, Oddball::kToStringOffset));
    Goto(&end);

    Bind(&not_oddball);
    {
      var_result.Bind(CallRuntime(Runtime::kToName, context, value));
      Goto(&end);
    }
  }

  Bind(&end);
  return var_result.value();
}

Node* CodeStubAssembler::NonNumberToNumber(Node* context, Node* input) {
  // Assert input is a HeapObject (not smi or heap number)
  CSA_ASSERT(this, Word32BinaryNot(TaggedIsSmi(input)));
  CSA_ASSERT(this, Word32NotEqual(LoadMap(input), HeapNumberMapConstant()));

  // We might need to loop once here due to ToPrimitive conversions.
  Variable var_input(this, MachineRepresentation::kTagged);
  Variable var_result(this, MachineRepresentation::kTagged);
  Label loop(this, &var_input);
  Label end(this);
  var_input.Bind(input);
  Goto(&loop);
  Bind(&loop);
  {
    // Load the current {input} value (known to be a HeapObject).
    Node* input = var_input.value();

    // Dispatch on the {input} instance type.
    Node* input_instance_type = LoadInstanceType(input);
    Label if_inputisstring(this), if_inputisoddball(this),
        if_inputisreceiver(this, Label::kDeferred),
        if_inputisother(this, Label::kDeferred);
    GotoIf(IsStringInstanceType(input_instance_type), &if_inputisstring);
    GotoIf(Word32Equal(input_instance_type, Int32Constant(ODDBALL_TYPE)),
           &if_inputisoddball);
    Branch(IsJSReceiverInstanceType(input_instance_type), &if_inputisreceiver,
           &if_inputisother);

    Bind(&if_inputisstring);
    {
      // The {input} is a String, use the fast stub to convert it to a Number.
      var_result.Bind(StringToNumber(context, input));
      Goto(&end);
    }

    Bind(&if_inputisoddball);
    {
      // The {input} is an Oddball, we just need to load the Number value of it.
      var_result.Bind(LoadObjectField(input, Oddball::kToNumberOffset));
      Goto(&end);
    }

    Bind(&if_inputisreceiver);
    {
      // The {input} is a JSReceiver, we need to convert it to a Primitive first
      // using the ToPrimitive type conversion, preferably yielding a Number.
      Callable callable = CodeFactory::NonPrimitiveToPrimitive(
          isolate(), ToPrimitiveHint::kNumber);
      Node* result = CallStub(callable, context, input);

      // Check if the {result} is already a Number.
      Label if_resultisnumber(this), if_resultisnotnumber(this);
      GotoIf(TaggedIsSmi(result), &if_resultisnumber);
      Node* result_map = LoadMap(result);
      Branch(WordEqual(result_map, HeapNumberMapConstant()), &if_resultisnumber,
             &if_resultisnotnumber);

      Bind(&if_resultisnumber);
      {
        // The ToPrimitive conversion already gave us a Number, so we're done.
        var_result.Bind(result);
        Goto(&end);
      }

      Bind(&if_resultisnotnumber);
      {
        // We now have a Primitive {result}, but it's not yet a Number.
        var_input.Bind(result);
        Goto(&loop);
      }
    }

    Bind(&if_inputisother);
    {
      // The {input} is something else (i.e. Symbol or Simd128Value), let the
      // runtime figure out the correct exception.
      // Note: We cannot tail call to the runtime here, as js-to-wasm
      // trampolines also use this code currently, and they declare all
      // outgoing parameters as untagged, while we would push a tagged
      // object here.
      var_result.Bind(CallRuntime(Runtime::kToNumber, context, input));
      Goto(&end);
    }
  }

  Bind(&end);
  return var_result.value();
}

Node* CodeStubAssembler::ToNumber(Node* context, Node* input) {
  Variable var_result(this, MachineRepresentation::kTagged);
  Label end(this);

  Label not_smi(this, Label::kDeferred);
  GotoUnless(TaggedIsSmi(input), &not_smi);
  var_result.Bind(input);
  Goto(&end);

  Bind(&not_smi);
  {
    Label not_heap_number(this, Label::kDeferred);
    Node* input_map = LoadMap(input);
    GotoIf(Word32NotEqual(input_map, HeapNumberMapConstant()),
           &not_heap_number);

    var_result.Bind(input);
    Goto(&end);

    Bind(&not_heap_number);
    {
      var_result.Bind(NonNumberToNumber(context, input));
      Goto(&end);
    }
  }

  Bind(&end);
  return var_result.value();
}

Node* CodeStubAssembler::ToString(Node* context, Node* input) {
  Label is_number(this);
  Label runtime(this, Label::kDeferred);
  Variable result(this, MachineRepresentation::kTagged);
  Label done(this, &result);

  GotoIf(TaggedIsSmi(input), &is_number);

  Node* input_map = LoadMap(input);
  Node* input_instance_type = LoadMapInstanceType(input_map);

  result.Bind(input);
  GotoIf(IsStringInstanceType(input_instance_type), &done);

  Label not_heap_number(this);
  Branch(WordNotEqual(input_map, HeapNumberMapConstant()), &not_heap_number,
         &is_number);

  Bind(&is_number);
  result.Bind(NumberToString(context, input));
  Goto(&done);

  Bind(&not_heap_number);
  {
    GotoIf(Word32NotEqual(input_instance_type, Int32Constant(ODDBALL_TYPE)),
           &runtime);
    result.Bind(LoadObjectField(input, Oddball::kToStringOffset));
    Goto(&done);
  }

  Bind(&runtime);
  {
    result.Bind(CallRuntime(Runtime::kToString, context, input));
    Goto(&done);
  }

  Bind(&done);
  return result.value();
}

Node* CodeStubAssembler::FlattenString(Node* string) {
  CSA_ASSERT(this, IsString(string));
  Variable var_result(this, MachineRepresentation::kTagged);
  var_result.Bind(string);

  Node* instance_type = LoadInstanceType(string);

  // Check if the {string} is not a ConsString (i.e. already flat).
  Label is_cons(this, Label::kDeferred), is_flat_in_cons(this), end(this);
  {
    GotoUnless(Word32Equal(Word32And(instance_type,
                                     Int32Constant(kStringRepresentationMask)),
                           Int32Constant(kConsStringTag)),
               &end);

    // Check whether the right hand side is the empty string (i.e. if
    // this is really a flat string in a cons string).
    Node* rhs = LoadObjectField(string, ConsString::kSecondOffset);
    Branch(WordEqual(rhs, EmptyStringConstant()), &is_flat_in_cons, &is_cons);
  }

  // Bail out to the runtime.
  Bind(&is_cons);
  {
    var_result.Bind(
        CallRuntime(Runtime::kFlattenString, NoContextConstant(), string));
    Goto(&end);
  }

  Bind(&is_flat_in_cons);
  {
    var_result.Bind(LoadObjectField(string, ConsString::kFirstOffset));
    Goto(&end);
  }

  Bind(&end);
  return var_result.value();
}

Node* CodeStubAssembler::JSReceiverToPrimitive(Node* context, Node* input) {
  Label if_isreceiver(this, Label::kDeferred), if_isnotreceiver(this);
  Variable result(this, MachineRepresentation::kTagged);
  Label done(this, &result);

  BranchIfJSReceiver(input, &if_isreceiver, &if_isnotreceiver);

  Bind(&if_isreceiver);
  {
    // Convert {input} to a primitive first passing Number hint.
    Callable callable = CodeFactory::NonPrimitiveToPrimitive(isolate());
    result.Bind(CallStub(callable, context, input));
    Goto(&done);
  }

  Bind(&if_isnotreceiver);
  {
    result.Bind(input);
    Goto(&done);
  }

  Bind(&done);
  return result.value();
}

Node* CodeStubAssembler::ToInteger(Node* context, Node* input,
                                   ToIntegerTruncationMode mode) {
  // We might need to loop once for ToNumber conversion.
  Variable var_arg(this, MachineRepresentation::kTagged);
  Label loop(this, &var_arg), out(this);
  var_arg.Bind(input);
  Goto(&loop);
  Bind(&loop);
  {
    // Shared entry points.
    Label return_zero(this, Label::kDeferred);

    // Load the current {arg} value.
    Node* arg = var_arg.value();

    // Check if {arg} is a Smi.
    GotoIf(TaggedIsSmi(arg), &out);

    // Check if {arg} is a HeapNumber.
    Label if_argisheapnumber(this),
        if_argisnotheapnumber(this, Label::kDeferred);
    Branch(WordEqual(LoadMap(arg), HeapNumberMapConstant()),
           &if_argisheapnumber, &if_argisnotheapnumber);

    Bind(&if_argisheapnumber);
    {
      // Load the floating-point value of {arg}.
      Node* arg_value = LoadHeapNumberValue(arg);

      // Check if {arg} is NaN.
      GotoUnless(Float64Equal(arg_value, arg_value), &return_zero);

      // Truncate {arg} towards zero.
      Node* value = Float64Trunc(arg_value);

      if (mode == kTruncateMinusZero) {
        // Truncate -0.0 to 0.
        GotoIf(Float64Equal(value, Float64Constant(0.0)), &return_zero);
      }

      var_arg.Bind(ChangeFloat64ToTagged(value));
      Goto(&out);
    }

    Bind(&if_argisnotheapnumber);
    {
      // Need to convert {arg} to a Number first.
      Callable callable = CodeFactory::NonNumberToNumber(isolate());
      var_arg.Bind(CallStub(callable, context, arg));
      Goto(&loop);
    }

    Bind(&return_zero);
    var_arg.Bind(SmiConstant(Smi::kZero));
    Goto(&out);
  }

  Bind(&out);
  return var_arg.value();
}

Node* CodeStubAssembler::DecodeWord32(Node* word32, uint32_t shift,
                                      uint32_t mask) {
  return Word32Shr(Word32And(word32, Int32Constant(mask)),
                   static_cast<int>(shift));
}

Node* CodeStubAssembler::DecodeWord(Node* word, uint32_t shift, uint32_t mask) {
  return WordShr(WordAnd(word, IntPtrConstant(mask)), static_cast<int>(shift));
}

void CodeStubAssembler::SetCounter(StatsCounter* counter, int value) {
  if (FLAG_native_code_counters && counter->Enabled()) {
    Node* counter_address = ExternalConstant(ExternalReference(counter));
    StoreNoWriteBarrier(MachineRepresentation::kWord32, counter_address,
                        Int32Constant(value));
  }
}

void CodeStubAssembler::IncrementCounter(StatsCounter* counter, int delta) {
  DCHECK(delta > 0);
  if (FLAG_native_code_counters && counter->Enabled()) {
    Node* counter_address = ExternalConstant(ExternalReference(counter));
    Node* value = Load(MachineType::Int32(), counter_address);
    value = Int32Add(value, Int32Constant(delta));
    StoreNoWriteBarrier(MachineRepresentation::kWord32, counter_address, value);
  }
}

void CodeStubAssembler::DecrementCounter(StatsCounter* counter, int delta) {
  DCHECK(delta > 0);
  if (FLAG_native_code_counters && counter->Enabled()) {
    Node* counter_address = ExternalConstant(ExternalReference(counter));
    Node* value = Load(MachineType::Int32(), counter_address);
    value = Int32Sub(value, Int32Constant(delta));
    StoreNoWriteBarrier(MachineRepresentation::kWord32, counter_address, value);
  }
}

void CodeStubAssembler::Use(Label* label) {
  GotoIf(Word32Equal(Int32Constant(0), Int32Constant(1)), label);
}

void CodeStubAssembler::TryToName(Node* key, Label* if_keyisindex,
                                  Variable* var_index, Label* if_keyisunique,
                                  Label* if_bailout) {
  DCHECK_EQ(MachineType::PointerRepresentation(), var_index->rep());
  Comment("TryToName");

  Label if_hascachedindex(this), if_keyisnotindex(this);
  // Handle Smi and HeapNumber keys.
  var_index->Bind(TryToIntptr(key, &if_keyisnotindex));
  Goto(if_keyisindex);

  Bind(&if_keyisnotindex);
  Node* key_instance_type = LoadInstanceType(key);
  // Symbols are unique.
  GotoIf(Word32Equal(key_instance_type, Int32Constant(SYMBOL_TYPE)),
         if_keyisunique);
  // Miss if |key| is not a String.
  STATIC_ASSERT(FIRST_NAME_TYPE == FIRST_TYPE);
  GotoUnless(IsStringInstanceType(key_instance_type), if_bailout);
  // |key| is a String. Check if it has a cached array index.
  Node* hash = LoadNameHashField(key);
  Node* contains_index =
      Word32And(hash, Int32Constant(Name::kContainsCachedArrayIndexMask));
  GotoIf(Word32Equal(contains_index, Int32Constant(0)), &if_hascachedindex);
  // No cached array index. If the string knows that it contains an index,
  // then it must be an uncacheable index. Handle this case in the runtime.
  Node* not_an_index =
      Word32And(hash, Int32Constant(Name::kIsNotArrayIndexMask));
  GotoIf(Word32Equal(not_an_index, Int32Constant(0)), if_bailout);
  // Finally, check if |key| is internalized.
  STATIC_ASSERT(kNotInternalizedTag != 0);
  Node* not_internalized =
      Word32And(key_instance_type, Int32Constant(kIsNotInternalizedMask));
  GotoIf(Word32NotEqual(not_internalized, Int32Constant(0)), if_bailout);
  Goto(if_keyisunique);

  Bind(&if_hascachedindex);
  var_index->Bind(DecodeWordFromWord32<Name::ArrayIndexValueBits>(hash));
  Goto(if_keyisindex);
}

template <typename Dictionary>
Node* CodeStubAssembler::EntryToIndex(Node* entry, int field_index) {
  Node* entry_index = IntPtrMul(entry, IntPtrConstant(Dictionary::kEntrySize));
  return IntPtrAdd(entry_index, IntPtrConstant(Dictionary::kElementsStartIndex +
                                               field_index));
}

template Node* CodeStubAssembler::EntryToIndex<NameDictionary>(Node*, int);
template Node* CodeStubAssembler::EntryToIndex<GlobalDictionary>(Node*, int);

Node* CodeStubAssembler::HashTableComputeCapacity(Node* at_least_space_for) {
  Node* capacity = IntPtrRoundUpToPowerOfTwo32(
      WordShl(at_least_space_for, IntPtrConstant(1)));
  return IntPtrMax(capacity, IntPtrConstant(HashTableBase::kMinCapacity));
}

Node* CodeStubAssembler::IntPtrMax(Node* left, Node* right) {
  return Select(IntPtrGreaterThanOrEqual(left, right), left, right,
                MachineType::PointerRepresentation());
}

template <typename Dictionary>
void CodeStubAssembler::NameDictionaryLookup(Node* dictionary,
                                             Node* unique_name, Label* if_found,
                                             Variable* var_name_index,
                                             Label* if_not_found,
                                             int inlined_probes) {
  CSA_ASSERT(this, IsDictionary(dictionary));
  DCHECK_EQ(MachineType::PointerRepresentation(), var_name_index->rep());
  Comment("NameDictionaryLookup");

  Node* capacity = SmiUntag(LoadFixedArrayElement(
      dictionary, IntPtrConstant(Dictionary::kCapacityIndex), 0,
      INTPTR_PARAMETERS));
  Node* mask = IntPtrSub(capacity, IntPtrConstant(1));
  Node* hash = ChangeUint32ToWord(LoadNameHash(unique_name));

  // See Dictionary::FirstProbe().
  Node* count = IntPtrConstant(0);
  Node* entry = WordAnd(hash, mask);

  for (int i = 0; i < inlined_probes; i++) {
    Node* index = EntryToIndex<Dictionary>(entry);
    var_name_index->Bind(index);

    Node* current =
        LoadFixedArrayElement(dictionary, index, 0, INTPTR_PARAMETERS);
    GotoIf(WordEqual(current, unique_name), if_found);

    // See Dictionary::NextProbe().
    count = IntPtrConstant(i + 1);
    entry = WordAnd(IntPtrAdd(entry, count), mask);
  }

  Node* undefined = UndefinedConstant();

  Variable var_count(this, MachineType::PointerRepresentation());
  Variable var_entry(this, MachineType::PointerRepresentation());
  Variable* loop_vars[] = {&var_count, &var_entry, var_name_index};
  Label loop(this, 3, loop_vars);
  var_count.Bind(count);
  var_entry.Bind(entry);
  Goto(&loop);
  Bind(&loop);
  {
    Node* count = var_count.value();
    Node* entry = var_entry.value();

    Node* index = EntryToIndex<Dictionary>(entry);
    var_name_index->Bind(index);

    Node* current =
        LoadFixedArrayElement(dictionary, index, 0, INTPTR_PARAMETERS);
    GotoIf(WordEqual(current, undefined), if_not_found);
    GotoIf(WordEqual(current, unique_name), if_found);

    // See Dictionary::NextProbe().
    count = IntPtrAdd(count, IntPtrConstant(1));
    entry = WordAnd(IntPtrAdd(entry, count), mask);

    var_count.Bind(count);
    var_entry.Bind(entry);
    Goto(&loop);
  }
}

// Instantiate template methods to workaround GCC compilation issue.
template void CodeStubAssembler::NameDictionaryLookup<NameDictionary>(
    Node*, Node*, Label*, Variable*, Label*, int);
template void CodeStubAssembler::NameDictionaryLookup<GlobalDictionary>(
    Node*, Node*, Label*, Variable*, Label*, int);

Node* CodeStubAssembler::ComputeIntegerHash(Node* key, Node* seed) {
  // See v8::internal::ComputeIntegerHash()
  Node* hash = key;
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

template <typename Dictionary>
void CodeStubAssembler::NumberDictionaryLookup(Node* dictionary,
                                               Node* intptr_index,
                                               Label* if_found,
                                               Variable* var_entry,
                                               Label* if_not_found) {
  CSA_ASSERT(this, IsDictionary(dictionary));
  DCHECK_EQ(MachineType::PointerRepresentation(), var_entry->rep());
  Comment("NumberDictionaryLookup");

  Node* capacity = SmiUntag(LoadFixedArrayElement(
      dictionary, IntPtrConstant(Dictionary::kCapacityIndex), 0,
      INTPTR_PARAMETERS));
  Node* mask = IntPtrSub(capacity, IntPtrConstant(1));

  Node* int32_seed;
  if (Dictionary::ShapeT::UsesSeed) {
    int32_seed = HashSeed();
  } else {
    int32_seed = Int32Constant(kZeroHashSeed);
  }
  Node* hash = ChangeUint32ToWord(ComputeIntegerHash(intptr_index, int32_seed));
  Node* key_as_float64 = RoundIntPtrToFloat64(intptr_index);

  // See Dictionary::FirstProbe().
  Node* count = IntPtrConstant(0);
  Node* entry = WordAnd(hash, mask);

  Node* undefined = UndefinedConstant();
  Node* the_hole = TheHoleConstant();

  Variable var_count(this, MachineType::PointerRepresentation());
  Variable* loop_vars[] = {&var_count, var_entry};
  Label loop(this, 2, loop_vars);
  var_count.Bind(count);
  var_entry->Bind(entry);
  Goto(&loop);
  Bind(&loop);
  {
    Node* count = var_count.value();
    Node* entry = var_entry->value();

    Node* index = EntryToIndex<Dictionary>(entry);
    Node* current =
        LoadFixedArrayElement(dictionary, index, 0, INTPTR_PARAMETERS);
    GotoIf(WordEqual(current, undefined), if_not_found);
    Label next_probe(this);
    {
      Label if_currentissmi(this), if_currentisnotsmi(this);
      Branch(TaggedIsSmi(current), &if_currentissmi, &if_currentisnotsmi);
      Bind(&if_currentissmi);
      {
        Node* current_value = SmiUntag(current);
        Branch(WordEqual(current_value, intptr_index), if_found, &next_probe);
      }
      Bind(&if_currentisnotsmi);
      {
        GotoIf(WordEqual(current, the_hole), &next_probe);
        // Current must be the Number.
        Node* current_value = LoadHeapNumberValue(current);
        Branch(Float64Equal(current_value, key_as_float64), if_found,
               &next_probe);
      }
    }

    Bind(&next_probe);
    // See Dictionary::NextProbe().
    count = IntPtrAdd(count, IntPtrConstant(1));
    entry = WordAnd(IntPtrAdd(entry, count), mask);

    var_count.Bind(count);
    var_entry->Bind(entry);
    Goto(&loop);
  }
}

void CodeStubAssembler::DescriptorLookupLinear(Node* unique_name,
                                               Node* descriptors, Node* nof,
                                               Label* if_found,
                                               Variable* var_name_index,
                                               Label* if_not_found) {
  Node* first_inclusive = IntPtrConstant(DescriptorArray::ToKeyIndex(0));
  Node* factor = IntPtrConstant(DescriptorArray::kDescriptorSize);
  Node* last_exclusive = IntPtrAdd(first_inclusive, IntPtrMul(nof, factor));

  BuildFastLoop(
      MachineType::PointerRepresentation(), last_exclusive, first_inclusive,
      [descriptors, unique_name, if_found, var_name_index](
          CodeStubAssembler* assembler, Node* name_index) {
        Node* candidate_name = assembler->LoadFixedArrayElement(
            descriptors, name_index, 0, INTPTR_PARAMETERS);
        var_name_index->Bind(name_index);
        assembler->GotoIf(assembler->WordEqual(candidate_name, unique_name),
                          if_found);
      },
      -DescriptorArray::kDescriptorSize, IndexAdvanceMode::kPre);
  Goto(if_not_found);
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
  Bind(&if_isfastmap);
  {
    Comment("DescriptorArrayLookup");
    Node* nof =
        DecodeWordFromWord32<Map::NumberOfOwnDescriptorsBits>(bit_field3);
    // Bail out to the runtime for large numbers of own descriptors. The stub
    // only does linear search, which becomes too expensive in that case.
    {
      static const int32_t kMaxLinear = 210;
      GotoIf(UintPtrGreaterThan(nof, IntPtrConstant(kMaxLinear)), if_bailout);
    }
    Node* descriptors = LoadMapDescriptors(map);
    var_meta_storage->Bind(descriptors);

    DescriptorLookupLinear(unique_name, descriptors, nof, if_found_fast,
                           var_name_index, if_not_found);
  }
  Bind(&if_isslowmap);
  {
    Node* dictionary = LoadProperties(object);
    var_meta_storage->Bind(dictionary);

    NameDictionaryLookup<NameDictionary>(dictionary, unique_name, if_found_dict,
                                         var_name_index, if_not_found);
  }
  Bind(&if_objectisspecial);
  {
    // Handle global object here and other special objects in runtime.
    GotoUnless(Word32Equal(instance_type, Int32Constant(JS_GLOBAL_OBJECT_TYPE)),
               if_bailout);

    // Handle interceptors and access checks in runtime.
    Node* bit_field = LoadMapBitField(map);
    Node* mask = Int32Constant(1 << Map::kHasNamedInterceptor |
                               1 << Map::kIsAccessCheckNeeded);
    GotoIf(Word32NotEqual(Word32And(bit_field, mask), Int32Constant(0)),
           if_bailout);

    Node* dictionary = LoadProperties(object);
    var_meta_storage->Bind(dictionary);

    NameDictionaryLookup<GlobalDictionary>(
        dictionary, unique_name, if_found_global, var_name_index, if_not_found);
  }
}

void CodeStubAssembler::TryHasOwnProperty(compiler::Node* object,
                                          compiler::Node* map,
                                          compiler::Node* instance_type,
                                          compiler::Node* unique_name,
                                          Label* if_found, Label* if_not_found,
                                          Label* if_bailout) {
  Comment("TryHasOwnProperty");
  Variable var_meta_storage(this, MachineRepresentation::kTagged);
  Variable var_name_index(this, MachineType::PointerRepresentation());

  Label if_found_global(this);
  TryLookupProperty(object, map, instance_type, unique_name, if_found, if_found,
                    &if_found_global, &var_meta_storage, &var_name_index,
                    if_not_found, if_bailout);
  Bind(&if_found_global);
  {
    Variable var_value(this, MachineRepresentation::kTagged);
    Variable var_details(this, MachineRepresentation::kWord32);
    // Check if the property cell is not deleted.
    LoadPropertyFromGlobalDictionary(var_meta_storage.value(),
                                     var_name_index.value(), &var_value,
                                     &var_details, if_not_found);
    Goto(if_found);
  }
}

void CodeStubAssembler::LoadPropertyFromFastObject(Node* object, Node* map,
                                                   Node* descriptors,
                                                   Node* name_index,
                                                   Variable* var_details,
                                                   Variable* var_value) {
  DCHECK_EQ(MachineRepresentation::kWord32, var_details->rep());
  DCHECK_EQ(MachineRepresentation::kTagged, var_value->rep());
  Comment("[ LoadPropertyFromFastObject");

  const int name_to_details_offset =
      (DescriptorArray::kDescriptorDetails - DescriptorArray::kDescriptorKey) *
      kPointerSize;
  const int name_to_value_offset =
      (DescriptorArray::kDescriptorValue - DescriptorArray::kDescriptorKey) *
      kPointerSize;

  Node* details = LoadAndUntagToWord32FixedArrayElement(descriptors, name_index,
                                                        name_to_details_offset);
  var_details->Bind(details);

  Node* location = DecodeWord32<PropertyDetails::LocationField>(details);

  Label if_in_field(this), if_in_descriptor(this), done(this);
  Branch(Word32Equal(location, Int32Constant(kField)), &if_in_field,
         &if_in_descriptor);
  Bind(&if_in_field);
  {
    Node* field_index =
        DecodeWordFromWord32<PropertyDetails::FieldIndexField>(details);
    Node* representation =
        DecodeWord32<PropertyDetails::RepresentationField>(details);

    Node* inobject_properties = LoadMapInobjectProperties(map);

    Label if_inobject(this), if_backing_store(this);
    Variable var_double_value(this, MachineRepresentation::kFloat64);
    Label rebox_double(this, &var_double_value);
    Branch(UintPtrLessThan(field_index, inobject_properties), &if_inobject,
           &if_backing_store);
    Bind(&if_inobject);
    {
      Comment("if_inobject");
      Node* field_offset =
          IntPtrMul(IntPtrSub(LoadMapInstanceSize(map),
                              IntPtrSub(inobject_properties, field_index)),
                    IntPtrConstant(kPointerSize));

      Label if_double(this), if_tagged(this);
      Branch(Word32NotEqual(representation,
                            Int32Constant(Representation::kDouble)),
             &if_tagged, &if_double);
      Bind(&if_tagged);
      {
        var_value->Bind(LoadObjectField(object, field_offset));
        Goto(&done);
      }
      Bind(&if_double);
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
    Bind(&if_backing_store);
    {
      Comment("if_backing_store");
      Node* properties = LoadProperties(object);
      field_index = IntPtrSub(field_index, inobject_properties);
      Node* value = LoadFixedArrayElement(properties, field_index);

      Label if_double(this), if_tagged(this);
      Branch(Word32NotEqual(representation,
                            Int32Constant(Representation::kDouble)),
             &if_tagged, &if_double);
      Bind(&if_tagged);
      {
        var_value->Bind(value);
        Goto(&done);
      }
      Bind(&if_double);
      {
        var_double_value.Bind(LoadHeapNumberValue(value));
        Goto(&rebox_double);
      }
    }
    Bind(&rebox_double);
    {
      Comment("rebox_double");
      Node* heap_number = AllocateHeapNumberWithValue(var_double_value.value());
      var_value->Bind(heap_number);
      Goto(&done);
    }
  }
  Bind(&if_in_descriptor);
  {
    Node* value =
        LoadFixedArrayElement(descriptors, name_index, name_to_value_offset);
    var_value->Bind(value);
    Goto(&done);
  }
  Bind(&done);

  Comment("] LoadPropertyFromFastObject");
}

void CodeStubAssembler::LoadPropertyFromNameDictionary(Node* dictionary,
                                                       Node* name_index,
                                                       Variable* var_details,
                                                       Variable* var_value) {
  Comment("LoadPropertyFromNameDictionary");
  CSA_ASSERT(this, IsDictionary(dictionary));
  const int name_to_details_offset =
      (NameDictionary::kEntryDetailsIndex - NameDictionary::kEntryKeyIndex) *
      kPointerSize;
  const int name_to_value_offset =
      (NameDictionary::kEntryValueIndex - NameDictionary::kEntryKeyIndex) *
      kPointerSize;

  Node* details = LoadAndUntagToWord32FixedArrayElement(dictionary, name_index,
                                                        name_to_details_offset);

  var_details->Bind(details);
  var_value->Bind(
      LoadFixedArrayElement(dictionary, name_index, name_to_value_offset));

  Comment("] LoadPropertyFromNameDictionary");
}

void CodeStubAssembler::LoadPropertyFromGlobalDictionary(Node* dictionary,
                                                         Node* name_index,
                                                         Variable* var_details,
                                                         Variable* var_value,
                                                         Label* if_deleted) {
  Comment("[ LoadPropertyFromGlobalDictionary");
  CSA_ASSERT(this, IsDictionary(dictionary));

  const int name_to_value_offset =
      (GlobalDictionary::kEntryValueIndex - GlobalDictionary::kEntryKeyIndex) *
      kPointerSize;

  Node* property_cell =
      LoadFixedArrayElement(dictionary, name_index, name_to_value_offset);

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
                                              Label* if_bailout) {
  Variable var_value(this, MachineRepresentation::kTagged);
  var_value.Bind(value);
  Label done(this);

  Node* kind = DecodeWord32<PropertyDetails::KindField>(details);
  GotoIf(Word32Equal(kind, Int32Constant(kData)), &done);

  // Accessor case.
  {
    Node* accessor_pair = value;
    GotoIf(Word32Equal(LoadInstanceType(accessor_pair),
                       Int32Constant(ACCESSOR_INFO_TYPE)),
           if_bailout);
    CSA_ASSERT(this, HasInstanceType(accessor_pair, ACCESSOR_PAIR_TYPE));
    Node* getter = LoadObjectField(accessor_pair, AccessorPair::kGetterOffset);
    Node* getter_map = LoadMap(getter);
    Node* instance_type = LoadMapInstanceType(getter_map);
    // FunctionTemplateInfo getters are not supported yet.
    GotoIf(
        Word32Equal(instance_type, Int32Constant(FUNCTION_TEMPLATE_INFO_TYPE)),
        if_bailout);

    // Return undefined if the {getter} is not callable.
    var_value.Bind(UndefinedConstant());
    GotoUnless(IsCallableMap(getter_map), &done);

    // Call the accessor.
    Callable callable = CodeFactory::Call(isolate());
    Node* result = CallJS(callable, context, getter, receiver);
    var_value.Bind(result);
    Goto(&done);
  }

  Bind(&done);
  return var_value.value();
}

void CodeStubAssembler::TryGetOwnProperty(
    Node* context, Node* receiver, Node* object, Node* map, Node* instance_type,
    Node* unique_name, Label* if_found_value, Variable* var_value,
    Label* if_not_found, Label* if_bailout) {
  DCHECK_EQ(MachineRepresentation::kTagged, var_value->rep());
  Comment("TryGetOwnProperty");

  Variable var_meta_storage(this, MachineRepresentation::kTagged);
  Variable var_entry(this, MachineType::PointerRepresentation());

  Label if_found_fast(this), if_found_dict(this), if_found_global(this);

  Variable var_details(this, MachineRepresentation::kWord32);
  Variable* vars[] = {var_value, &var_details};
  Label if_found(this, 2, vars);

  TryLookupProperty(object, map, instance_type, unique_name, &if_found_fast,
                    &if_found_dict, &if_found_global, &var_meta_storage,
                    &var_entry, if_not_found, if_bailout);
  Bind(&if_found_fast);
  {
    Node* descriptors = var_meta_storage.value();
    Node* name_index = var_entry.value();

    LoadPropertyFromFastObject(object, map, descriptors, name_index,
                               &var_details, var_value);
    Goto(&if_found);
  }
  Bind(&if_found_dict);
  {
    Node* dictionary = var_meta_storage.value();
    Node* entry = var_entry.value();
    LoadPropertyFromNameDictionary(dictionary, entry, &var_details, var_value);
    Goto(&if_found);
  }
  Bind(&if_found_global);
  {
    Node* dictionary = var_meta_storage.value();
    Node* entry = var_entry.value();

    LoadPropertyFromGlobalDictionary(dictionary, entry, &var_details, var_value,
                                     if_not_found);
    Goto(&if_found);
  }
  // Here we have details and value which could be an accessor.
  Bind(&if_found);
  {
    Node* value = CallGetterIfAccessor(var_value->value(), var_details.value(),
                                       context, receiver, if_bailout);
    var_value->Bind(value);
    Goto(if_found_value);
  }
}

void CodeStubAssembler::TryLookupElement(Node* object, Node* map,
                                         Node* instance_type,
                                         Node* intptr_index, Label* if_found,
                                         Label* if_not_found,
                                         Label* if_bailout) {
  // Handle special objects in runtime.
  GotoIf(Int32LessThanOrEqual(instance_type,
                              Int32Constant(LAST_SPECIAL_RECEIVER_TYPE)),
         if_bailout);

  Node* elements_kind = LoadMapElementsKind(map);

  // TODO(verwaest): Support other elements kinds as well.
  Label if_isobjectorsmi(this), if_isdouble(this), if_isdictionary(this),
      if_isfaststringwrapper(this), if_isslowstringwrapper(this), if_oob(this);
  // clang-format off
  int32_t values[] = {
      // Handled by {if_isobjectorsmi}.
      FAST_SMI_ELEMENTS, FAST_HOLEY_SMI_ELEMENTS, FAST_ELEMENTS,
          FAST_HOLEY_ELEMENTS,
      // Handled by {if_isdouble}.
      FAST_DOUBLE_ELEMENTS, FAST_HOLEY_DOUBLE_ELEMENTS,
      // Handled by {if_isdictionary}.
      DICTIONARY_ELEMENTS,
      // Handled by {if_isfaststringwrapper}.
      FAST_STRING_WRAPPER_ELEMENTS,
      // Handled by {if_isslowstringwrapper}.
      SLOW_STRING_WRAPPER_ELEMENTS,
      // Handled by {if_not_found}.
      NO_ELEMENTS,
  };
  Label* labels[] = {
      &if_isobjectorsmi, &if_isobjectorsmi, &if_isobjectorsmi,
          &if_isobjectorsmi,
      &if_isdouble, &if_isdouble,
      &if_isdictionary,
      &if_isfaststringwrapper,
      &if_isslowstringwrapper,
      if_not_found,
  };
  // clang-format on
  STATIC_ASSERT(arraysize(values) == arraysize(labels));
  Switch(elements_kind, if_bailout, values, labels, arraysize(values));

  Bind(&if_isobjectorsmi);
  {
    Node* elements = LoadElements(object);
    Node* length = LoadAndUntagFixedArrayBaseLength(elements);

    GotoUnless(UintPtrLessThan(intptr_index, length), &if_oob);

    Node* element =
        LoadFixedArrayElement(elements, intptr_index, 0, INTPTR_PARAMETERS);
    Node* the_hole = TheHoleConstant();
    Branch(WordEqual(element, the_hole), if_not_found, if_found);
  }
  Bind(&if_isdouble);
  {
    Node* elements = LoadElements(object);
    Node* length = LoadAndUntagFixedArrayBaseLength(elements);

    GotoUnless(UintPtrLessThan(intptr_index, length), &if_oob);

    // Check if the element is a double hole, but don't load it.
    LoadFixedDoubleArrayElement(elements, intptr_index, MachineType::None(), 0,
                                INTPTR_PARAMETERS, if_not_found);
    Goto(if_found);
  }
  Bind(&if_isdictionary);
  {
    // Negative keys must be converted to property names.
    GotoIf(IntPtrLessThan(intptr_index, IntPtrConstant(0)), if_bailout);

    Variable var_entry(this, MachineType::PointerRepresentation());
    Node* elements = LoadElements(object);
    NumberDictionaryLookup<SeededNumberDictionary>(
        elements, intptr_index, if_found, &var_entry, if_not_found);
  }
  Bind(&if_isfaststringwrapper);
  {
    CSA_ASSERT(this, HasInstanceType(object, JS_VALUE_TYPE));
    Node* string = LoadJSValueValue(object);
    CSA_ASSERT(this, IsStringInstanceType(LoadInstanceType(string)));
    Node* length = LoadStringLength(string);
    GotoIf(UintPtrLessThan(intptr_index, SmiUntag(length)), if_found);
    Goto(&if_isobjectorsmi);
  }
  Bind(&if_isslowstringwrapper);
  {
    CSA_ASSERT(this, HasInstanceType(object, JS_VALUE_TYPE));
    Node* string = LoadJSValueValue(object);
    CSA_ASSERT(this, IsStringInstanceType(LoadInstanceType(string)));
    Node* length = LoadStringLength(string);
    GotoIf(UintPtrLessThan(intptr_index, SmiUntag(length)), if_found);
    Goto(&if_isdictionary);
  }
  Bind(&if_oob);
  {
    // Positive OOB indices mean "not found", negative indices must be
    // converted to property names.
    GotoIf(IntPtrLessThan(intptr_index, IntPtrConstant(0)), if_bailout);
    Goto(if_not_found);
  }
}

// Instantiate template methods to workaround GCC compilation issue.
template void CodeStubAssembler::NumberDictionaryLookup<SeededNumberDictionary>(
    Node*, Node*, Label*, Variable*, Label*);
template void CodeStubAssembler::NumberDictionaryLookup<
    UnseededNumberDictionary>(Node*, Node*, Label*, Variable*, Label*);

void CodeStubAssembler::TryPrototypeChainLookup(
    Node* receiver, Node* key, LookupInHolder& lookup_property_in_holder,
    LookupInHolder& lookup_element_in_holder, Label* if_end,
    Label* if_bailout) {
  // Ensure receiver is JSReceiver, otherwise bailout.
  Label if_objectisnotsmi(this);
  Branch(TaggedIsSmi(receiver), if_bailout, &if_objectisnotsmi);
  Bind(&if_objectisnotsmi);

  Node* map = LoadMap(receiver);
  Node* instance_type = LoadMapInstanceType(map);
  {
    Label if_objectisreceiver(this);
    STATIC_ASSERT(LAST_JS_RECEIVER_TYPE == LAST_TYPE);
    STATIC_ASSERT(FIRST_JS_RECEIVER_TYPE == JS_PROXY_TYPE);
    Branch(
        Int32GreaterThan(instance_type, Int32Constant(FIRST_JS_RECEIVER_TYPE)),
        &if_objectisreceiver, if_bailout);
    Bind(&if_objectisreceiver);
  }

  Variable var_index(this, MachineType::PointerRepresentation());

  Label if_keyisindex(this), if_iskeyunique(this);
  TryToName(key, &if_keyisindex, &var_index, &if_iskeyunique, if_bailout);

  Bind(&if_iskeyunique);
  {
    Variable var_holder(this, MachineRepresentation::kTagged);
    Variable var_holder_map(this, MachineRepresentation::kTagged);
    Variable var_holder_instance_type(this, MachineRepresentation::kWord8);

    Variable* merged_variables[] = {&var_holder, &var_holder_map,
                                    &var_holder_instance_type};
    Label loop(this, arraysize(merged_variables), merged_variables);
    var_holder.Bind(receiver);
    var_holder_map.Bind(map);
    var_holder_instance_type.Bind(instance_type);
    Goto(&loop);
    Bind(&loop);
    {
      Node* holder_map = var_holder_map.value();
      Node* holder_instance_type = var_holder_instance_type.value();

      Label next_proto(this);
      lookup_property_in_holder(receiver, var_holder.value(), holder_map,
                                holder_instance_type, key, &next_proto,
                                if_bailout);
      Bind(&next_proto);

      // Bailout if it can be an integer indexed exotic case.
      GotoIf(
          Word32Equal(holder_instance_type, Int32Constant(JS_TYPED_ARRAY_TYPE)),
          if_bailout);

      Node* proto = LoadMapPrototype(holder_map);

      Label if_not_null(this);
      Branch(WordEqual(proto, NullConstant()), if_end, &if_not_null);
      Bind(&if_not_null);

      Node* map = LoadMap(proto);
      Node* instance_type = LoadMapInstanceType(map);

      var_holder.Bind(proto);
      var_holder_map.Bind(map);
      var_holder_instance_type.Bind(instance_type);
      Goto(&loop);
    }
  }
  Bind(&if_keyisindex);
  {
    Variable var_holder(this, MachineRepresentation::kTagged);
    Variable var_holder_map(this, MachineRepresentation::kTagged);
    Variable var_holder_instance_type(this, MachineRepresentation::kWord8);

    Variable* merged_variables[] = {&var_holder, &var_holder_map,
                                    &var_holder_instance_type};
    Label loop(this, arraysize(merged_variables), merged_variables);
    var_holder.Bind(receiver);
    var_holder_map.Bind(map);
    var_holder_instance_type.Bind(instance_type);
    Goto(&loop);
    Bind(&loop);
    {
      Label next_proto(this);
      lookup_element_in_holder(receiver, var_holder.value(),
                               var_holder_map.value(),
                               var_holder_instance_type.value(),
                               var_index.value(), &next_proto, if_bailout);
      Bind(&next_proto);

      Node* proto = LoadMapPrototype(var_holder_map.value());

      Label if_not_null(this);
      Branch(WordEqual(proto, NullConstant()), if_end, &if_not_null);
      Bind(&if_not_null);

      Node* map = LoadMap(proto);
      Node* instance_type = LoadMapInstanceType(map);

      var_holder.Bind(proto);
      var_holder_map.Bind(map);
      var_holder_instance_type.Bind(instance_type);
      Goto(&loop);
    }
  }
}

Node* CodeStubAssembler::OrdinaryHasInstance(Node* context, Node* callable,
                                             Node* object) {
  Variable var_result(this, MachineRepresentation::kTagged);
  Label return_false(this), return_true(this),
      return_runtime(this, Label::kDeferred), return_result(this);

  // Goto runtime if {object} is a Smi.
  GotoIf(TaggedIsSmi(object), &return_runtime);

  // Load map of {object}.
  Node* object_map = LoadMap(object);

  // Lookup the {callable} and {object} map in the global instanceof cache.
  // Note: This is safe because we clear the global instanceof cache whenever
  // we change the prototype of any object.
  Node* instanceof_cache_function =
      LoadRoot(Heap::kInstanceofCacheFunctionRootIndex);
  Node* instanceof_cache_map = LoadRoot(Heap::kInstanceofCacheMapRootIndex);
  {
    Label instanceof_cache_miss(this);
    GotoUnless(WordEqual(instanceof_cache_function, callable),
               &instanceof_cache_miss);
    GotoUnless(WordEqual(instanceof_cache_map, object_map),
               &instanceof_cache_miss);
    var_result.Bind(LoadRoot(Heap::kInstanceofCacheAnswerRootIndex));
    Goto(&return_result);
    Bind(&instanceof_cache_miss);
  }

  // Goto runtime if {callable} is a Smi.
  GotoIf(TaggedIsSmi(callable), &return_runtime);

  // Load map of {callable}.
  Node* callable_map = LoadMap(callable);

  // Goto runtime if {callable} is not a JSFunction.
  Node* callable_instance_type = LoadMapInstanceType(callable_map);
  GotoUnless(
      Word32Equal(callable_instance_type, Int32Constant(JS_FUNCTION_TYPE)),
      &return_runtime);

  // Goto runtime if {callable} is not a constructor or has
  // a non-instance "prototype".
  Node* callable_bitfield = LoadMapBitField(callable_map);
  GotoUnless(
      Word32Equal(Word32And(callable_bitfield,
                            Int32Constant((1 << Map::kHasNonInstancePrototype) |
                                          (1 << Map::kIsConstructor))),
                  Int32Constant(1 << Map::kIsConstructor)),
      &return_runtime);

  // Get the "prototype" (or initial map) of the {callable}.
  Node* callable_prototype =
      LoadObjectField(callable, JSFunction::kPrototypeOrInitialMapOffset);
  {
    Variable var_callable_prototype(this, MachineRepresentation::kTagged);
    Label callable_prototype_valid(this);
    var_callable_prototype.Bind(callable_prototype);

    // Resolve the "prototype" if the {callable} has an initial map.  Afterwards
    // the {callable_prototype} will be either the JSReceiver prototype object
    // or the hole value, which means that no instances of the {callable} were
    // created so far and hence we should return false.
    Node* callable_prototype_instance_type =
        LoadInstanceType(callable_prototype);
    GotoUnless(
        Word32Equal(callable_prototype_instance_type, Int32Constant(MAP_TYPE)),
        &callable_prototype_valid);
    var_callable_prototype.Bind(
        LoadObjectField(callable_prototype, Map::kPrototypeOffset));
    Goto(&callable_prototype_valid);
    Bind(&callable_prototype_valid);
    callable_prototype = var_callable_prototype.value();
  }

  // Update the global instanceof cache with the current {object} map and
  // {callable}.  The cached answer will be set when it is known below.
  StoreRoot(Heap::kInstanceofCacheFunctionRootIndex, callable);
  StoreRoot(Heap::kInstanceofCacheMapRootIndex, object_map);

  // Loop through the prototype chain looking for the {callable} prototype.
  Variable var_object_map(this, MachineRepresentation::kTagged);
  var_object_map.Bind(object_map);
  Label loop(this, &var_object_map);
  Goto(&loop);
  Bind(&loop);
  {
    Node* object_map = var_object_map.value();

    // Check if the current {object} needs to be access checked.
    Node* object_bitfield = LoadMapBitField(object_map);
    GotoUnless(
        Word32Equal(Word32And(object_bitfield,
                              Int32Constant(1 << Map::kIsAccessCheckNeeded)),
                    Int32Constant(0)),
        &return_runtime);

    // Check if the current {object} is a proxy.
    Node* object_instance_type = LoadMapInstanceType(object_map);
    GotoIf(Word32Equal(object_instance_type, Int32Constant(JS_PROXY_TYPE)),
           &return_runtime);

    // Check the current {object} prototype.
    Node* object_prototype = LoadMapPrototype(object_map);
    GotoIf(WordEqual(object_prototype, NullConstant()), &return_false);
    GotoIf(WordEqual(object_prototype, callable_prototype), &return_true);

    // Continue with the prototype.
    var_object_map.Bind(LoadMap(object_prototype));
    Goto(&loop);
  }

  Bind(&return_true);
  StoreRoot(Heap::kInstanceofCacheAnswerRootIndex, BooleanConstant(true));
  var_result.Bind(BooleanConstant(true));
  Goto(&return_result);

  Bind(&return_false);
  StoreRoot(Heap::kInstanceofCacheAnswerRootIndex, BooleanConstant(false));
  var_result.Bind(BooleanConstant(false));
  Goto(&return_result);

  Bind(&return_runtime);
  {
    // Invalidate the global instanceof cache.
    StoreRoot(Heap::kInstanceofCacheFunctionRootIndex, SmiConstant(0));
    // Fallback to the runtime implementation.
    var_result.Bind(
        CallRuntime(Runtime::kOrdinaryHasInstance, context, callable, object));
  }
  Goto(&return_result);

  Bind(&return_result);
  return var_result.value();
}

compiler::Node* CodeStubAssembler::ElementOffsetFromIndex(Node* index_node,
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
  } else if (mode == INTEGER_PARAMETERS) {
    int32_t temp = 0;
    constant_index = ToInt32Constant(index_node, temp);
    index = static_cast<intptr_t>(temp);
  } else {
    DCHECK(mode == INTPTR_PARAMETERS);
    constant_index = ToIntPtrConstant(index_node, index);
  }
  if (constant_index) {
    return IntPtrConstant(base_size + element_size * index);
  }
  if (Is64() && mode == INTEGER_PARAMETERS) {
    index_node = ChangeInt32ToInt64(index_node);
  }

  Node* shifted_index =
      (element_size_shift == 0)
          ? index_node
          : ((element_size_shift > 0)
                 ? WordShl(index_node, IntPtrConstant(element_size_shift))
                 : WordShr(index_node, IntPtrConstant(-element_size_shift)));
  return IntPtrAddFoldConstants(IntPtrConstant(base_size), shifted_index);
}

compiler::Node* CodeStubAssembler::LoadTypeFeedbackVectorForStub() {
  Node* function =
      LoadFromParentFrame(JavaScriptFrameConstants::kFunctionOffset);
  Node* literals = LoadObjectField(function, JSFunction::kLiteralsOffset);
  return LoadObjectField(literals, LiteralsArray::kFeedbackVectorOffset);
}

void CodeStubAssembler::UpdateFeedback(compiler::Node* feedback,
                                       compiler::Node* type_feedback_vector,
                                       compiler::Node* slot_id) {
  // This method is used for binary op and compare feedback. These
  // vector nodes are initialized with a smi 0, so we can simply OR
  // our new feedback in place.
  // TODO(interpreter): Consider passing the feedback as Smi already to avoid
  // the tagging completely.
  Node* previous_feedback =
      LoadFixedArrayElement(type_feedback_vector, slot_id);
  Node* combined_feedback = SmiOr(previous_feedback, SmiFromWord32(feedback));
  StoreFixedArrayElement(type_feedback_vector, slot_id, combined_feedback,
                         SKIP_WRITE_BARRIER);
}

compiler::Node* CodeStubAssembler::LoadReceiverMap(compiler::Node* receiver) {
  Variable var_receiver_map(this, MachineRepresentation::kTagged);
  Label load_smi_map(this, Label::kDeferred), load_receiver_map(this),
      if_result(this);

  Branch(TaggedIsSmi(receiver), &load_smi_map, &load_receiver_map);
  Bind(&load_smi_map);
  {
    var_receiver_map.Bind(LoadRoot(Heap::kHeapNumberMapRootIndex));
    Goto(&if_result);
  }
  Bind(&load_receiver_map);
  {
    var_receiver_map.Bind(LoadMap(receiver));
    Goto(&if_result);
  }
  Bind(&if_result);
  return var_receiver_map.value();
}

compiler::Node* CodeStubAssembler::TryMonomorphicCase(
    compiler::Node* slot, compiler::Node* vector, compiler::Node* receiver_map,
    Label* if_handler, Variable* var_handler, Label* if_miss) {
  Comment("TryMonomorphicCase");
  DCHECK_EQ(MachineRepresentation::kTagged, var_handler->rep());

  // TODO(ishell): add helper class that hides offset computations for a series
  // of loads.
  int32_t header_size = FixedArray::kHeaderSize - kHeapObjectTag;
  // Adding |header_size| with a separate IntPtrAdd rather than passing it
  // into ElementOffsetFromIndex() allows it to be folded into a single
  // [base, index, offset] indirect memory access on x64.
  Node* offset =
      ElementOffsetFromIndex(slot, FAST_HOLEY_ELEMENTS, SMI_PARAMETERS);
  Node* feedback = Load(MachineType::AnyTagged(), vector,
                        IntPtrAdd(offset, IntPtrConstant(header_size)));

  // Try to quickly handle the monomorphic case without knowing for sure
  // if we have a weak cell in feedback. We do know it's safe to look
  // at WeakCell::kValueOffset.
  GotoIf(WordNotEqual(receiver_map, LoadWeakCellValueUnchecked(feedback)),
         if_miss);

  Node* handler =
      Load(MachineType::AnyTagged(), vector,
           IntPtrAdd(offset, IntPtrConstant(header_size + kPointerSize)));

  var_handler->Bind(handler);
  Goto(if_handler);
  return feedback;
}

void CodeStubAssembler::HandlePolymorphicCase(
    compiler::Node* receiver_map, compiler::Node* feedback, Label* if_handler,
    Variable* var_handler, Label* if_miss, int unroll_count) {
  Comment("HandlePolymorphicCase");
  DCHECK_EQ(MachineRepresentation::kTagged, var_handler->rep());

  // Iterate {feedback} array.
  const int kEntrySize = 2;

  for (int i = 0; i < unroll_count; i++) {
    Label next_entry(this);
    Node* cached_map = LoadWeakCellValue(LoadFixedArrayElement(
        feedback, IntPtrConstant(i * kEntrySize), 0, INTPTR_PARAMETERS));
    GotoIf(WordNotEqual(receiver_map, cached_map), &next_entry);

    // Found, now call handler.
    Node* handler = LoadFixedArrayElement(
        feedback, IntPtrConstant(i * kEntrySize + 1), 0, INTPTR_PARAMETERS);
    var_handler->Bind(handler);
    Goto(if_handler);

    Bind(&next_entry);
  }

  // Loop from {unroll_count}*kEntrySize to {length}.
  Node* init = IntPtrConstant(unroll_count * kEntrySize);
  Node* length = LoadAndUntagFixedArrayBaseLength(feedback);
  BuildFastLoop(
      MachineType::PointerRepresentation(), init, length,
      [receiver_map, feedback, if_handler, var_handler](CodeStubAssembler* csa,
                                                        Node* index) {
        Node* cached_map = csa->LoadWeakCellValue(
            csa->LoadFixedArrayElement(feedback, index, 0, INTPTR_PARAMETERS));

        Label next_entry(csa);
        csa->GotoIf(csa->WordNotEqual(receiver_map, cached_map), &next_entry);

        // Found, now call handler.
        Node* handler = csa->LoadFixedArrayElement(
            feedback, index, kPointerSize, INTPTR_PARAMETERS);
        var_handler->Bind(handler);
        csa->Goto(if_handler);

        csa->Bind(&next_entry);
      },
      kEntrySize, IndexAdvanceMode::kPost);
  // The loop falls through if no handler was found.
  Goto(if_miss);
}

void CodeStubAssembler::HandleKeyedStorePolymorphicCase(
    compiler::Node* receiver_map, compiler::Node* feedback, Label* if_handler,
    Variable* var_handler, Label* if_transition_handler,
    Variable* var_transition_map_cell, Label* if_miss) {
  DCHECK_EQ(MachineRepresentation::kTagged, var_handler->rep());
  DCHECK_EQ(MachineRepresentation::kTagged, var_transition_map_cell->rep());

  const int kEntrySize = 3;

  Node* init = IntPtrConstant(0);
  Node* length = LoadAndUntagFixedArrayBaseLength(feedback);
  BuildFastLoop(
      MachineType::PointerRepresentation(), init, length,
      [receiver_map, feedback, if_handler, var_handler, if_transition_handler,
       var_transition_map_cell](CodeStubAssembler* csa, Node* index) {
        Node* cached_map = csa->LoadWeakCellValue(
            csa->LoadFixedArrayElement(feedback, index, 0, INTPTR_PARAMETERS));
        Label next_entry(csa);
        csa->GotoIf(csa->WordNotEqual(receiver_map, cached_map), &next_entry);

        Node* maybe_transition_map_cell = csa->LoadFixedArrayElement(
            feedback, index, kPointerSize, INTPTR_PARAMETERS);

        var_handler->Bind(csa->LoadFixedArrayElement(
            feedback, index, 2 * kPointerSize, INTPTR_PARAMETERS));
        csa->GotoIf(
            csa->WordEqual(maybe_transition_map_cell,
                           csa->LoadRoot(Heap::kUndefinedValueRootIndex)),
            if_handler);
        var_transition_map_cell->Bind(maybe_transition_map_cell);
        csa->Goto(if_transition_handler);

        csa->Bind(&next_entry);
      },
      kEntrySize, IndexAdvanceMode::kPost);
  // The loop falls through if no handler was found.
  Goto(if_miss);
}

compiler::Node* CodeStubAssembler::StubCachePrimaryOffset(compiler::Node* name,
                                                          compiler::Node* map) {
  // See v8::internal::StubCache::PrimaryOffset().
  STATIC_ASSERT(StubCache::kCacheIndexShift == Name::kHashShift);
  // Compute the hash of the name (use entire hash field).
  Node* hash_field = LoadNameHashField(name);
  CSA_ASSERT(this,
             Word32Equal(Word32And(hash_field,
                                   Int32Constant(Name::kHashNotComputedMask)),
                         Int32Constant(0)));

  // Using only the low bits in 64-bit mode is unlikely to increase the
  // risk of collision even if the heap is spread over an area larger than
  // 4Gb (and not at all if it isn't).
  Node* hash = Int32Add(hash_field, map);
  // Base the offset on a simple combination of name and map.
  hash = Word32Xor(hash, Int32Constant(StubCache::kPrimaryMagic));
  uint32_t mask = (StubCache::kPrimaryTableSize - 1)
                  << StubCache::kCacheIndexShift;
  return ChangeUint32ToWord(Word32And(hash, Int32Constant(mask)));
}

compiler::Node* CodeStubAssembler::StubCacheSecondaryOffset(
    compiler::Node* name, compiler::Node* seed) {
  // See v8::internal::StubCache::SecondaryOffset().

  // Use the seed from the primary cache in the secondary cache.
  Node* hash = Int32Sub(seed, name);
  hash = Int32Add(hash, Int32Constant(StubCache::kSecondaryMagic));
  int32_t mask = (StubCache::kSecondaryTableSize - 1)
                 << StubCache::kCacheIndexShift;
  return ChangeUint32ToWord(Word32And(hash, Int32Constant(mask)));
}

enum CodeStubAssembler::StubCacheTable : int {
  kPrimary = static_cast<int>(StubCache::kPrimary),
  kSecondary = static_cast<int>(StubCache::kSecondary)
};

void CodeStubAssembler::TryProbeStubCacheTable(
    StubCache* stub_cache, StubCacheTable table_id,
    compiler::Node* entry_offset, compiler::Node* name, compiler::Node* map,
    Label* if_handler, Variable* var_handler, Label* if_miss) {
  StubCache::Table table = static_cast<StubCache::Table>(table_id);
#ifdef DEBUG
  if (FLAG_test_secondary_stub_cache && table == StubCache::kPrimary) {
    Goto(if_miss);
    return;
  } else if (FLAG_test_primary_stub_cache && table == StubCache::kSecondary) {
    Goto(if_miss);
    return;
  }
#endif
  // The {table_offset} holds the entry offset times four (due to masking
  // and shifting optimizations).
  const int kMultiplier = sizeof(StubCache::Entry) >> Name::kHashShift;
  entry_offset = IntPtrMul(entry_offset, IntPtrConstant(kMultiplier));

  // Check that the key in the entry matches the name.
  Node* key_base =
      ExternalConstant(ExternalReference(stub_cache->key_reference(table)));
  Node* entry_key = Load(MachineType::Pointer(), key_base, entry_offset);
  GotoIf(WordNotEqual(name, entry_key), if_miss);

  // Get the map entry from the cache.
  DCHECK_EQ(kPointerSize * 2, stub_cache->map_reference(table).address() -
                                  stub_cache->key_reference(table).address());
  Node* entry_map =
      Load(MachineType::Pointer(), key_base,
           IntPtrAdd(entry_offset, IntPtrConstant(kPointerSize * 2)));
  GotoIf(WordNotEqual(map, entry_map), if_miss);

  DCHECK_EQ(kPointerSize, stub_cache->value_reference(table).address() -
                              stub_cache->key_reference(table).address());
  Node* handler = Load(MachineType::TaggedPointer(), key_base,
                       IntPtrAdd(entry_offset, IntPtrConstant(kPointerSize)));

  // We found the handler.
  var_handler->Bind(handler);
  Goto(if_handler);
}

void CodeStubAssembler::TryProbeStubCache(
    StubCache* stub_cache, compiler::Node* receiver, compiler::Node* name,
    Label* if_handler, Variable* var_handler, Label* if_miss) {
  Label try_secondary(this), miss(this);

  Counters* counters = isolate()->counters();
  IncrementCounter(counters->megamorphic_stub_cache_probes(), 1);

  // Check that the {receiver} isn't a smi.
  GotoIf(TaggedIsSmi(receiver), &miss);

  Node* receiver_map = LoadMap(receiver);

  // Probe the primary table.
  Node* primary_offset = StubCachePrimaryOffset(name, receiver_map);
  TryProbeStubCacheTable(stub_cache, kPrimary, primary_offset, name,
                         receiver_map, if_handler, var_handler, &try_secondary);

  Bind(&try_secondary);
  {
    // Probe the secondary table.
    Node* secondary_offset = StubCacheSecondaryOffset(name, primary_offset);
    TryProbeStubCacheTable(stub_cache, kSecondary, secondary_offset, name,
                           receiver_map, if_handler, var_handler, &miss);
  }

  Bind(&miss);
  {
    IncrementCounter(counters->megamorphic_stub_cache_misses(), 1);
    Goto(if_miss);
  }
}

Node* CodeStubAssembler::TryToIntptr(Node* key, Label* miss) {
  Variable var_intptr_key(this, MachineType::PointerRepresentation());
  Label done(this, &var_intptr_key), key_is_smi(this);
  GotoIf(TaggedIsSmi(key), &key_is_smi);
  // Try to convert a heap number to a Smi.
  GotoUnless(WordEqual(LoadMap(key), HeapNumberMapConstant()), miss);
  {
    Node* value = LoadHeapNumberValue(key);
    Node* int_value = RoundFloat64ToInt32(value);
    GotoUnless(Float64Equal(value, ChangeInt32ToFloat64(int_value)), miss);
    var_intptr_key.Bind(ChangeInt32ToIntPtr(int_value));
    Goto(&done);
  }

  Bind(&key_is_smi);
  {
    var_intptr_key.Bind(SmiUntag(key));
    Goto(&done);
  }

  Bind(&done);
  return var_intptr_key.value();
}

void CodeStubAssembler::EmitFastElementsBoundsCheck(Node* object,
                                                    Node* elements,
                                                    Node* intptr_index,
                                                    Node* is_jsarray_condition,
                                                    Label* miss) {
  Variable var_length(this, MachineType::PointerRepresentation());
  Comment("Fast elements bounds check");
  Label if_array(this), length_loaded(this, &var_length);
  GotoIf(is_jsarray_condition, &if_array);
  {
    var_length.Bind(SmiUntag(LoadFixedArrayBaseLength(elements)));
    Goto(&length_loaded);
  }
  Bind(&if_array);
  {
    var_length.Bind(SmiUntag(LoadJSArrayLength(object)));
    Goto(&length_loaded);
  }
  Bind(&length_loaded);
  GotoUnless(UintPtrLessThan(intptr_index, var_length.value()), miss);
}

void CodeStubAssembler::EmitElementLoad(Node* object, Node* elements,
                                        Node* elements_kind, Node* intptr_index,
                                        Node* is_jsarray_condition,
                                        Label* if_hole, Label* rebox_double,
                                        Variable* var_double_value,
                                        Label* unimplemented_elements_kind,
                                        Label* out_of_bounds, Label* miss) {
  Label if_typed_array(this), if_fast_packed(this), if_fast_holey(this),
      if_fast_double(this), if_fast_holey_double(this), if_nonfast(this),
      if_dictionary(this);
  GotoIf(
      IntPtrGreaterThan(elements_kind, IntPtrConstant(LAST_FAST_ELEMENTS_KIND)),
      &if_nonfast);

  EmitFastElementsBoundsCheck(object, elements, intptr_index,
                              is_jsarray_condition, out_of_bounds);
  int32_t kinds[] = {// Handled by if_fast_packed.
                     FAST_SMI_ELEMENTS, FAST_ELEMENTS,
                     // Handled by if_fast_holey.
                     FAST_HOLEY_SMI_ELEMENTS, FAST_HOLEY_ELEMENTS,
                     // Handled by if_fast_double.
                     FAST_DOUBLE_ELEMENTS,
                     // Handled by if_fast_holey_double.
                     FAST_HOLEY_DOUBLE_ELEMENTS};
  Label* labels[] = {// FAST_{SMI,}_ELEMENTS
                     &if_fast_packed, &if_fast_packed,
                     // FAST_HOLEY_{SMI,}_ELEMENTS
                     &if_fast_holey, &if_fast_holey,
                     // FAST_DOUBLE_ELEMENTS
                     &if_fast_double,
                     // FAST_HOLEY_DOUBLE_ELEMENTS
                     &if_fast_holey_double};
  Switch(elements_kind, unimplemented_elements_kind, kinds, labels,
         arraysize(kinds));

  Bind(&if_fast_packed);
  {
    Comment("fast packed elements");
    Return(LoadFixedArrayElement(elements, intptr_index, 0, INTPTR_PARAMETERS));
  }

  Bind(&if_fast_holey);
  {
    Comment("fast holey elements");
    Node* element =
        LoadFixedArrayElement(elements, intptr_index, 0, INTPTR_PARAMETERS);
    GotoIf(WordEqual(element, TheHoleConstant()), if_hole);
    Return(element);
  }

  Bind(&if_fast_double);
  {
    Comment("packed double elements");
    var_double_value->Bind(LoadFixedDoubleArrayElement(
        elements, intptr_index, MachineType::Float64(), 0, INTPTR_PARAMETERS));
    Goto(rebox_double);
  }

  Bind(&if_fast_holey_double);
  {
    Comment("holey double elements");
    Node* value = LoadFixedDoubleArrayElement(elements, intptr_index,
                                              MachineType::Float64(), 0,
                                              INTPTR_PARAMETERS, if_hole);
    var_double_value->Bind(value);
    Goto(rebox_double);
  }

  Bind(&if_nonfast);
  {
    STATIC_ASSERT(LAST_ELEMENTS_KIND == LAST_FIXED_TYPED_ARRAY_ELEMENTS_KIND);
    GotoIf(IntPtrGreaterThanOrEqual(
               elements_kind,
               IntPtrConstant(FIRST_FIXED_TYPED_ARRAY_ELEMENTS_KIND)),
           &if_typed_array);
    GotoIf(IntPtrEqual(elements_kind, IntPtrConstant(DICTIONARY_ELEMENTS)),
           &if_dictionary);
    Goto(unimplemented_elements_kind);
  }

  Bind(&if_dictionary);
  {
    Comment("dictionary elements");
    GotoIf(IntPtrLessThan(intptr_index, IntPtrConstant(0)), out_of_bounds);
    Variable var_entry(this, MachineType::PointerRepresentation());
    Label if_found(this);
    NumberDictionaryLookup<SeededNumberDictionary>(
        elements, intptr_index, &if_found, &var_entry, if_hole);
    Bind(&if_found);
    // Check that the value is a data property.
    Node* details_index = EntryToIndex<SeededNumberDictionary>(
        var_entry.value(), SeededNumberDictionary::kEntryDetailsIndex);
    Node* details = SmiToWord32(
        LoadFixedArrayElement(elements, details_index, 0, INTPTR_PARAMETERS));
    Node* kind = DecodeWord32<PropertyDetails::KindField>(details);
    // TODO(jkummerow): Support accessors without missing?
    GotoUnless(Word32Equal(kind, Int32Constant(kData)), miss);
    // Finally, load the value.
    Node* value_index = EntryToIndex<SeededNumberDictionary>(
        var_entry.value(), SeededNumberDictionary::kEntryValueIndex);
    Return(LoadFixedArrayElement(elements, value_index, 0, INTPTR_PARAMETERS));
  }

  Bind(&if_typed_array);
  {
    Comment("typed elements");
    // Check if buffer has been neutered.
    Node* buffer = LoadObjectField(object, JSArrayBufferView::kBufferOffset);
    Node* bitfield = LoadObjectField(buffer, JSArrayBuffer::kBitFieldOffset,
                                     MachineType::Uint32());
    Node* neutered_bit =
        Word32And(bitfield, Int32Constant(JSArrayBuffer::WasNeutered::kMask));
    GotoUnless(Word32Equal(neutered_bit, Int32Constant(0)), miss);

    // Bounds check.
    Node* length =
        SmiUntag(LoadObjectField(object, JSTypedArray::kLengthOffset));
    GotoUnless(UintPtrLessThan(intptr_index, length), out_of_bounds);

    // Backing store = external_pointer + base_pointer.
    Node* external_pointer =
        LoadObjectField(elements, FixedTypedArrayBase::kExternalPointerOffset,
                        MachineType::Pointer());
    Node* base_pointer =
        LoadObjectField(elements, FixedTypedArrayBase::kBasePointerOffset);
    Node* backing_store = IntPtrAdd(external_pointer, base_pointer);

    Label uint8_elements(this), int8_elements(this), uint16_elements(this),
        int16_elements(this), uint32_elements(this), int32_elements(this),
        float32_elements(this), float64_elements(this);
    Label* elements_kind_labels[] = {
        &uint8_elements,  &uint8_elements,   &int8_elements,
        &uint16_elements, &int16_elements,   &uint32_elements,
        &int32_elements,  &float32_elements, &float64_elements};
    int32_t elements_kinds[] = {
        UINT8_ELEMENTS,  UINT8_CLAMPED_ELEMENTS, INT8_ELEMENTS,
        UINT16_ELEMENTS, INT16_ELEMENTS,         UINT32_ELEMENTS,
        INT32_ELEMENTS,  FLOAT32_ELEMENTS,       FLOAT64_ELEMENTS};
    const size_t kTypedElementsKindCount =
        LAST_FIXED_TYPED_ARRAY_ELEMENTS_KIND -
        FIRST_FIXED_TYPED_ARRAY_ELEMENTS_KIND + 1;
    DCHECK_EQ(kTypedElementsKindCount, arraysize(elements_kinds));
    DCHECK_EQ(kTypedElementsKindCount, arraysize(elements_kind_labels));
    Switch(elements_kind, miss, elements_kinds, elements_kind_labels,
           kTypedElementsKindCount);
    Bind(&uint8_elements);
    {
      Comment("UINT8_ELEMENTS");  // Handles UINT8_CLAMPED_ELEMENTS too.
      Return(SmiTag(Load(MachineType::Uint8(), backing_store, intptr_index)));
    }
    Bind(&int8_elements);
    {
      Comment("INT8_ELEMENTS");
      Return(SmiTag(Load(MachineType::Int8(), backing_store, intptr_index)));
    }
    Bind(&uint16_elements);
    {
      Comment("UINT16_ELEMENTS");
      Node* index = WordShl(intptr_index, IntPtrConstant(1));
      Return(SmiTag(Load(MachineType::Uint16(), backing_store, index)));
    }
    Bind(&int16_elements);
    {
      Comment("INT16_ELEMENTS");
      Node* index = WordShl(intptr_index, IntPtrConstant(1));
      Return(SmiTag(Load(MachineType::Int16(), backing_store, index)));
    }
    Bind(&uint32_elements);
    {
      Comment("UINT32_ELEMENTS");
      Node* index = WordShl(intptr_index, IntPtrConstant(2));
      Node* element = Load(MachineType::Uint32(), backing_store, index);
      Return(ChangeUint32ToTagged(element));
    }
    Bind(&int32_elements);
    {
      Comment("INT32_ELEMENTS");
      Node* index = WordShl(intptr_index, IntPtrConstant(2));
      Node* element = Load(MachineType::Int32(), backing_store, index);
      Return(ChangeInt32ToTagged(element));
    }
    Bind(&float32_elements);
    {
      Comment("FLOAT32_ELEMENTS");
      Node* index = WordShl(intptr_index, IntPtrConstant(2));
      Node* element = Load(MachineType::Float32(), backing_store, index);
      var_double_value->Bind(ChangeFloat32ToFloat64(element));
      Goto(rebox_double);
    }
    Bind(&float64_elements);
    {
      Comment("FLOAT64_ELEMENTS");
      Node* index = WordShl(intptr_index, IntPtrConstant(3));
      Node* element = Load(MachineType::Float64(), backing_store, index);
      var_double_value->Bind(element);
      Goto(rebox_double);
    }
  }
}

void CodeStubAssembler::HandleLoadICHandlerCase(
    const LoadICParameters* p, Node* handler, Label* miss,
    ElementSupport support_elements) {
  Comment("have_handler");
  Variable var_holder(this, MachineRepresentation::kTagged);
  var_holder.Bind(p->receiver);
  Variable var_smi_handler(this, MachineRepresentation::kTagged);
  var_smi_handler.Bind(handler);

  Variable* vars[] = {&var_holder, &var_smi_handler};
  Label if_smi_handler(this, 2, vars);
  Label try_proto_handler(this), call_handler(this);

  Branch(TaggedIsSmi(handler), &if_smi_handler, &try_proto_handler);

  // |handler| is a Smi, encoding what to do. See SmiHandler methods
  // for the encoding format.
  Bind(&if_smi_handler);
  {
    HandleLoadICSmiHandlerCase(p, var_holder.value(), var_smi_handler.value(),
                               miss, support_elements);
  }

  Bind(&try_proto_handler);
  {
    GotoIf(IsCodeMap(LoadMap(handler)), &call_handler);
    HandleLoadICProtoHandler(p, handler, &var_holder, &var_smi_handler,
                             &if_smi_handler, miss);
  }

  Bind(&call_handler);
  {
    typedef LoadWithVectorDescriptor Descriptor;
    TailCallStub(Descriptor(isolate()), handler, p->context,
                 Arg(Descriptor::kReceiver, p->receiver),
                 Arg(Descriptor::kName, p->name),
                 Arg(Descriptor::kSlot, p->slot),
                 Arg(Descriptor::kVector, p->vector));
  }
}

void CodeStubAssembler::HandleLoadICSmiHandlerCase(
    const LoadICParameters* p, Node* holder, Node* smi_handler, Label* miss,
    ElementSupport support_elements) {
  Variable var_double_value(this, MachineRepresentation::kFloat64);
  Label rebox_double(this, &var_double_value);

  Node* handler_word = SmiUntag(smi_handler);
  Node* handler_kind = DecodeWord<LoadHandler::KindBits>(handler_word);
  if (support_elements == kSupportElements) {
    Label property(this);
    GotoUnless(
        WordEqual(handler_kind, IntPtrConstant(LoadHandler::kForElements)),
        &property);

    Comment("element_load");
    Node* intptr_index = TryToIntptr(p->name, miss);
    Node* elements = LoadElements(holder);
    Node* is_jsarray_condition =
        IsSetWord<LoadHandler::IsJsArrayBits>(handler_word);
    Node* elements_kind =
        DecodeWord<LoadHandler::ElementsKindBits>(handler_word);
    Label if_hole(this), unimplemented_elements_kind(this);
    Label* out_of_bounds = miss;
    EmitElementLoad(holder, elements, elements_kind, intptr_index,
                    is_jsarray_condition, &if_hole, &rebox_double,
                    &var_double_value, &unimplemented_elements_kind,
                    out_of_bounds, miss);

    Bind(&unimplemented_elements_kind);
    {
      // Smi handlers should only be installed for supported elements kinds.
      // Crash if we get here.
      DebugBreak();
      Goto(miss);
    }

    Bind(&if_hole);
    {
      Comment("convert hole");
      GotoUnless(IsSetWord<LoadHandler::ConvertHoleBits>(handler_word), miss);
      Node* protector_cell = LoadRoot(Heap::kArrayProtectorRootIndex);
      DCHECK(isolate()->heap()->array_protector()->IsPropertyCell());
      GotoUnless(
          WordEqual(LoadObjectField(protector_cell, PropertyCell::kValueOffset),
                    SmiConstant(Smi::FromInt(Isolate::kProtectorValid))),
          miss);
      Return(UndefinedConstant());
    }

    Bind(&property);
    Comment("property_load");
  }

  Label constant(this), field(this);
  Branch(WordEqual(handler_kind, IntPtrConstant(LoadHandler::kForFields)),
         &field, &constant);

  Bind(&field);
  {
    Comment("field_load");
    Node* offset = DecodeWord<LoadHandler::FieldOffsetBits>(handler_word);

    Label inobject(this), out_of_object(this);
    Branch(IsSetWord<LoadHandler::IsInobjectBits>(handler_word), &inobject,
           &out_of_object);

    Bind(&inobject);
    {
      Label is_double(this);
      GotoIf(IsSetWord<LoadHandler::IsDoubleBits>(handler_word), &is_double);
      Return(LoadObjectField(holder, offset));

      Bind(&is_double);
      if (FLAG_unbox_double_fields) {
        var_double_value.Bind(
            LoadObjectField(holder, offset, MachineType::Float64()));
      } else {
        Node* mutable_heap_number = LoadObjectField(holder, offset);
        var_double_value.Bind(LoadHeapNumberValue(mutable_heap_number));
      }
      Goto(&rebox_double);
    }

    Bind(&out_of_object);
    {
      Label is_double(this);
      Node* properties = LoadProperties(holder);
      Node* value = LoadObjectField(properties, offset);
      GotoIf(IsSetWord<LoadHandler::IsDoubleBits>(handler_word), &is_double);
      Return(value);

      Bind(&is_double);
      var_double_value.Bind(LoadHeapNumberValue(value));
      Goto(&rebox_double);
    }

    Bind(&rebox_double);
    Return(AllocateHeapNumberWithValue(var_double_value.value()));
  }

  Bind(&constant);
  {
    Comment("constant_load");
    Node* descriptors = LoadMapDescriptors(LoadMap(holder));
    Node* descriptor =
        DecodeWord<LoadHandler::DescriptorValueIndexBits>(handler_word);
    CSA_ASSERT(this,
               UintPtrLessThan(descriptor,
                               LoadAndUntagFixedArrayBaseLength(descriptors)));
    Node* value =
        LoadFixedArrayElement(descriptors, descriptor, 0, INTPTR_PARAMETERS);

    Label if_accessor_info(this);
    GotoIf(IsSetWord<LoadHandler::IsAccessorInfoBits>(handler_word),
           &if_accessor_info);
    Return(value);

    Bind(&if_accessor_info);
    Callable callable = CodeFactory::ApiGetter(isolate());
    TailCallStub(callable, p->context, p->receiver, holder, value);
  }
}

void CodeStubAssembler::HandleLoadICProtoHandler(
    const LoadICParameters* p, Node* handler, Variable* var_holder,
    Variable* var_smi_handler, Label* if_smi_handler, Label* miss) {
  DCHECK_EQ(MachineRepresentation::kTagged, var_holder->rep());
  DCHECK_EQ(MachineRepresentation::kTagged, var_smi_handler->rep());

  // IC dispatchers rely on these assumptions to be held.
  STATIC_ASSERT(FixedArray::kLengthOffset == LoadHandler::kHolderCellOffset);
  DCHECK_EQ(FixedArray::OffsetOfElementAt(LoadHandler::kSmiHandlerIndex),
            LoadHandler::kSmiHandlerOffset);
  DCHECK_EQ(FixedArray::OffsetOfElementAt(LoadHandler::kValidityCellIndex),
            LoadHandler::kValidityCellOffset);

  // Both FixedArray and Tuple3 handlers have validity cell at the same offset.
  Label validity_cell_check_done(this);
  Node* validity_cell =
      LoadObjectField(handler, LoadHandler::kValidityCellOffset);
  GotoIf(WordEqual(validity_cell, IntPtrConstant(0)),
         &validity_cell_check_done);
  Node* cell_value = LoadObjectField(validity_cell, Cell::kValueOffset);
  GotoIf(WordNotEqual(cell_value,
                      SmiConstant(Smi::FromInt(Map::kPrototypeChainValid))),
         miss);
  Goto(&validity_cell_check_done);

  Bind(&validity_cell_check_done);
  Node* smi_handler = LoadObjectField(handler, LoadHandler::kSmiHandlerOffset);
  CSA_ASSERT(this, TaggedIsSmi(smi_handler));
  Node* handler_flags = SmiUntag(smi_handler);

  Label check_prototypes(this);
  GotoUnless(
      IsSetWord<LoadHandler::DoNegativeLookupOnReceiverBits>(handler_flags),
      &check_prototypes);
  {
    CSA_ASSERT(this, Word32BinaryNot(
                         HasInstanceType(p->receiver, JS_GLOBAL_OBJECT_TYPE)));
    // We have a dictionary receiver, do a negative lookup check.
    NameDictionaryNegativeLookup(p->receiver, p->name, miss);
    Goto(&check_prototypes);
  }

  Bind(&check_prototypes);
  Node* maybe_holder_cell =
      LoadObjectField(handler, LoadHandler::kHolderCellOffset);
  Label array_handler(this), tuple_handler(this);
  Branch(TaggedIsSmi(maybe_holder_cell), &array_handler, &tuple_handler);

  Bind(&tuple_handler);
  {
    Label load_existent(this);
    GotoIf(WordNotEqual(maybe_holder_cell, NullConstant()), &load_existent);
    // This is a handler for a load of a non-existent value.
    Return(UndefinedConstant());

    Bind(&load_existent);
    Node* holder = LoadWeakCellValue(maybe_holder_cell);
    // The |holder| is guaranteed to be alive at this point since we passed
    // both the receiver map check and the validity cell check.
    CSA_ASSERT(this, WordNotEqual(holder, IntPtrConstant(0)));

    var_holder->Bind(holder);
    var_smi_handler->Bind(smi_handler);
    Goto(if_smi_handler);
  }

  Bind(&array_handler);
  {
    typedef LoadICProtoArrayDescriptor Descriptor;
    LoadICProtoArrayStub stub(isolate());
    Node* target = HeapConstant(stub.GetCode());
    TailCallStub(Descriptor(isolate()), target, p->context,
                 Arg(Descriptor::kReceiver, p->receiver),
                 Arg(Descriptor::kName, p->name),
                 Arg(Descriptor::kSlot, p->slot),
                 Arg(Descriptor::kVector, p->vector),
                 Arg(Descriptor::kHandler, handler));
  }
}

void CodeStubAssembler::LoadICProtoArray(const LoadICParameters* p,
                                         Node* handler) {
  Label miss(this);
  CSA_ASSERT(this, Word32BinaryNot(TaggedIsSmi(handler)));
  CSA_ASSERT(this, IsFixedArrayMap(LoadMap(handler)));

  Node* smi_handler = LoadObjectField(handler, LoadHandler::kSmiHandlerOffset);
  Node* handler_flags = SmiUntag(smi_handler);

  Node* handler_length = LoadAndUntagFixedArrayBaseLength(handler);

  Node* holder = EmitLoadICProtoArrayCheck(p, handler, handler_length,
                                           handler_flags, &miss);

  HandleLoadICSmiHandlerCase(p, holder, smi_handler, &miss, kOnlyProperties);

  Bind(&miss);
  {
    TailCallRuntime(Runtime::kLoadIC_Miss, p->context, p->receiver, p->name,
                    p->slot, p->vector);
  }
}

Node* CodeStubAssembler::EmitLoadICProtoArrayCheck(const LoadICParameters* p,
                                                   Node* handler,
                                                   Node* handler_length,
                                                   Node* handler_flags,
                                                   Label* miss) {
  Variable start_index(this, MachineType::PointerRepresentation());
  start_index.Bind(IntPtrConstant(LoadHandler::kFirstPrototypeIndex));

  Label can_access(this);
  GotoUnless(IsSetWord<LoadHandler::DoAccessCheckOnReceiverBits>(handler_flags),
             &can_access);
  {
    // Skip this entry of a handler.
    start_index.Bind(IntPtrConstant(LoadHandler::kFirstPrototypeIndex + 1));

    int offset =
        FixedArray::OffsetOfElementAt(LoadHandler::kFirstPrototypeIndex);
    Node* expected_native_context =
        LoadWeakCellValue(LoadObjectField(handler, offset), miss);
    CSA_ASSERT(this, IsNativeContext(expected_native_context));

    Node* native_context = LoadNativeContext(p->context);
    GotoIf(WordEqual(expected_native_context, native_context), &can_access);
    // If the receiver is not a JSGlobalProxy then we miss.
    GotoUnless(IsJSGlobalProxy(p->receiver), miss);
    // For JSGlobalProxy receiver try to compare security tokens of current
    // and expected native contexts.
    Node* expected_token = LoadContextElement(expected_native_context,
                                              Context::SECURITY_TOKEN_INDEX);
    Node* current_token =
        LoadContextElement(native_context, Context::SECURITY_TOKEN_INDEX);
    Branch(WordEqual(expected_token, current_token), &can_access, miss);
  }
  Bind(&can_access);

  BuildFastLoop(
      MachineType::PointerRepresentation(), start_index.value(), handler_length,
      [this, p, handler, miss](CodeStubAssembler*, Node* current) {
        Node* prototype_cell =
            LoadFixedArrayElement(handler, current, 0, INTPTR_PARAMETERS);
        CheckPrototype(prototype_cell, p->name, miss);
      },
      1, IndexAdvanceMode::kPost);

  Node* maybe_holder_cell = LoadFixedArrayElement(
      handler, IntPtrConstant(LoadHandler::kHolderCellIndex), 0,
      INTPTR_PARAMETERS);
  Label load_existent(this);
  GotoIf(WordNotEqual(maybe_holder_cell, NullConstant()), &load_existent);
  // This is a handler for a load of a non-existent value.
  Return(UndefinedConstant());

  Bind(&load_existent);
  Node* holder = LoadWeakCellValue(maybe_holder_cell);
  // The |holder| is guaranteed to be alive at this point since we passed
  // the receiver map check, the validity cell check and the prototype chain
  // check.
  CSA_ASSERT(this, WordNotEqual(holder, IntPtrConstant(0)));
  return holder;
}

void CodeStubAssembler::CheckPrototype(Node* prototype_cell, Node* name,
                                       Label* miss) {
  Node* maybe_prototype = LoadWeakCellValue(prototype_cell, miss);

  Label done(this);
  Label if_property_cell(this), if_dictionary_object(this);

  // |maybe_prototype| is either a PropertyCell or a slow-mode prototype.
  Branch(WordEqual(LoadMap(maybe_prototype),
                   LoadRoot(Heap::kGlobalPropertyCellMapRootIndex)),
         &if_property_cell, &if_dictionary_object);

  Bind(&if_dictionary_object);
  {
    CSA_ASSERT(this, IsDictionaryMap(LoadMap(maybe_prototype)));
    NameDictionaryNegativeLookup(maybe_prototype, name, miss);
    Goto(&done);
  }

  Bind(&if_property_cell);
  {
    // Ensure the property cell still contains the hole.
    Node* value = LoadObjectField(maybe_prototype, PropertyCell::kValueOffset);
    GotoIf(WordNotEqual(value, LoadRoot(Heap::kTheHoleValueRootIndex)), miss);
    Goto(&done);
  }

  Bind(&done);
}

void CodeStubAssembler::NameDictionaryNegativeLookup(Node* object, Node* name,
                                                     Label* miss) {
  CSA_ASSERT(this, IsDictionaryMap(LoadMap(object)));
  Node* properties = LoadProperties(object);
  // Ensure the property does not exist in a dictionary-mode object.
  Variable var_name_index(this, MachineType::PointerRepresentation());
  Label done(this);
  NameDictionaryLookup<NameDictionary>(properties, name, miss, &var_name_index,
                                       &done);
  Bind(&done);
}

void CodeStubAssembler::LoadIC(const LoadICParameters* p) {
  Variable var_handler(this, MachineRepresentation::kTagged);
  // TODO(ishell): defer blocks when it works.
  Label if_handler(this, &var_handler), try_polymorphic(this),
      try_megamorphic(this /*, Label::kDeferred*/),
      miss(this /*, Label::kDeferred*/);

  Node* receiver_map = LoadReceiverMap(p->receiver);

  // Check monomorphic case.
  Node* feedback =
      TryMonomorphicCase(p->slot, p->vector, receiver_map, &if_handler,
                         &var_handler, &try_polymorphic);
  Bind(&if_handler);
  {
    HandleLoadICHandlerCase(p, var_handler.value(), &miss);
  }

  Bind(&try_polymorphic);
  {
    // Check polymorphic case.
    Comment("LoadIC_try_polymorphic");
    GotoUnless(WordEqual(LoadMap(feedback), FixedArrayMapConstant()),
               &try_megamorphic);
    HandlePolymorphicCase(receiver_map, feedback, &if_handler, &var_handler,
                          &miss, 2);
  }

  Bind(&try_megamorphic);
  {
    // Check megamorphic case.
    GotoUnless(
        WordEqual(feedback, LoadRoot(Heap::kmegamorphic_symbolRootIndex)),
        &miss);

    TryProbeStubCache(isolate()->load_stub_cache(), p->receiver, p->name,
                      &if_handler, &var_handler, &miss);
  }
  Bind(&miss);
  {
    TailCallRuntime(Runtime::kLoadIC_Miss, p->context, p->receiver, p->name,
                    p->slot, p->vector);
  }
}

void CodeStubAssembler::KeyedLoadIC(const LoadICParameters* p) {
  Variable var_handler(this, MachineRepresentation::kTagged);
  // TODO(ishell): defer blocks when it works.
  Label if_handler(this, &var_handler), try_polymorphic(this),
      try_megamorphic(this /*, Label::kDeferred*/),
      try_polymorphic_name(this /*, Label::kDeferred*/),
      miss(this /*, Label::kDeferred*/);

  Node* receiver_map = LoadReceiverMap(p->receiver);

  // Check monomorphic case.
  Node* feedback =
      TryMonomorphicCase(p->slot, p->vector, receiver_map, &if_handler,
                         &var_handler, &try_polymorphic);
  Bind(&if_handler);
  {
    HandleLoadICHandlerCase(p, var_handler.value(), &miss, kSupportElements);
  }

  Bind(&try_polymorphic);
  {
    // Check polymorphic case.
    Comment("KeyedLoadIC_try_polymorphic");
    GotoUnless(WordEqual(LoadMap(feedback), FixedArrayMapConstant()),
               &try_megamorphic);
    HandlePolymorphicCase(receiver_map, feedback, &if_handler, &var_handler,
                          &miss, 2);
  }

  Bind(&try_megamorphic);
  {
    // Check megamorphic case.
    Comment("KeyedLoadIC_try_megamorphic");
    GotoUnless(
        WordEqual(feedback, LoadRoot(Heap::kmegamorphic_symbolRootIndex)),
        &try_polymorphic_name);
    // TODO(jkummerow): Inline this? Or some of it?
    TailCallStub(CodeFactory::KeyedLoadIC_Megamorphic(isolate()), p->context,
                 p->receiver, p->name, p->slot, p->vector);
  }
  Bind(&try_polymorphic_name);
  {
    // We might have a name in feedback, and a fixed array in the next slot.
    Comment("KeyedLoadIC_try_polymorphic_name");
    GotoUnless(WordEqual(feedback, p->name), &miss);
    // If the name comparison succeeded, we know we have a fixed array with
    // at least one map/handler pair.
    Node* offset = ElementOffsetFromIndex(
        p->slot, FAST_HOLEY_ELEMENTS, SMI_PARAMETERS,
        FixedArray::kHeaderSize + kPointerSize - kHeapObjectTag);
    Node* array = Load(MachineType::AnyTagged(), p->vector, offset);
    HandlePolymorphicCase(receiver_map, array, &if_handler, &var_handler, &miss,
                          1);
  }
  Bind(&miss);
  {
    Comment("KeyedLoadIC_miss");
    TailCallRuntime(Runtime::kKeyedLoadIC_Miss, p->context, p->receiver,
                    p->name, p->slot, p->vector);
  }
}

void CodeStubAssembler::KeyedLoadICGeneric(const LoadICParameters* p) {
  Variable var_index(this, MachineType::PointerRepresentation());
  Variable var_details(this, MachineRepresentation::kWord32);
  Variable var_value(this, MachineRepresentation::kTagged);
  Label if_index(this), if_unique_name(this), if_element_hole(this),
      if_oob(this), slow(this), stub_cache_miss(this),
      if_property_dictionary(this), if_found_on_receiver(this);

  Node* receiver = p->receiver;
  GotoIf(TaggedIsSmi(receiver), &slow);
  Node* receiver_map = LoadMap(receiver);
  Node* instance_type = LoadMapInstanceType(receiver_map);
  // Receivers requiring non-standard element accesses (interceptors, access
  // checks, strings and string wrappers, proxies) are handled in the runtime.
  GotoIf(Int32LessThanOrEqual(instance_type,
                              Int32Constant(LAST_CUSTOM_ELEMENTS_RECEIVER)),
         &slow);

  Node* key = p->name;
  TryToName(key, &if_index, &var_index, &if_unique_name, &slow);

  Bind(&if_index);
  {
    Comment("integer index");
    Node* index = var_index.value();
    Node* elements = LoadElements(receiver);
    Node* elements_kind = LoadMapElementsKind(receiver_map);
    Node* is_jsarray_condition =
        Word32Equal(instance_type, Int32Constant(JS_ARRAY_TYPE));
    Variable var_double_value(this, MachineRepresentation::kFloat64);
    Label rebox_double(this, &var_double_value);

    // Unimplemented elements kinds fall back to a runtime call.
    Label* unimplemented_elements_kind = &slow;
    IncrementCounter(isolate()->counters()->ic_keyed_load_generic_smi(), 1);
    EmitElementLoad(receiver, elements, elements_kind, index,
                    is_jsarray_condition, &if_element_hole, &rebox_double,
                    &var_double_value, unimplemented_elements_kind, &if_oob,
                    &slow);

    Bind(&rebox_double);
    Return(AllocateHeapNumberWithValue(var_double_value.value()));
  }

  Bind(&if_oob);
  {
    Comment("out of bounds");
    Node* index = var_index.value();
    // Negative keys can't take the fast OOB path.
    GotoIf(IntPtrLessThan(index, IntPtrConstant(0)), &slow);
    // Positive OOB indices are effectively the same as hole loads.
    Goto(&if_element_hole);
  }

  Bind(&if_element_hole);
  {
    Comment("found the hole");
    Label return_undefined(this);
    BranchIfPrototypesHaveNoElements(receiver_map, &return_undefined, &slow);

    Bind(&return_undefined);
    Return(UndefinedConstant());
  }

  Node* properties = nullptr;
  Bind(&if_unique_name);
  {
    Comment("key is unique name");
    // Check if the receiver has fast or slow properties.
    properties = LoadProperties(receiver);
    Node* properties_map = LoadMap(properties);
    GotoIf(WordEqual(properties_map, LoadRoot(Heap::kHashTableMapRootIndex)),
           &if_property_dictionary);

    // Try looking up the property on the receiver; if unsuccessful, look
    // for a handler in the stub cache.
    Comment("DescriptorArray lookup");

    // Skip linear search if there are too many descriptors.
    // TODO(jkummerow): Consider implementing binary search.
    // See also TryLookupProperty() which has the same limitation.
    const int32_t kMaxLinear = 210;
    Label stub_cache(this);
    Node* bitfield3 = LoadMapBitField3(receiver_map);
    Node* nof =
        DecodeWordFromWord32<Map::NumberOfOwnDescriptorsBits>(bitfield3);
    GotoIf(UintPtrGreaterThan(nof, IntPtrConstant(kMaxLinear)), &stub_cache);
    Node* descriptors = LoadMapDescriptors(receiver_map);
    Variable var_name_index(this, MachineType::PointerRepresentation());
    Label if_descriptor_found(this);
    DescriptorLookupLinear(key, descriptors, nof, &if_descriptor_found,
                           &var_name_index, &stub_cache);

    Bind(&if_descriptor_found);
    {
      LoadPropertyFromFastObject(receiver, receiver_map, descriptors,
                                 var_name_index.value(), &var_details,
                                 &var_value);
      Goto(&if_found_on_receiver);
    }

    Bind(&stub_cache);
    {
      Comment("stub cache probe for fast property load");
      Variable var_handler(this, MachineRepresentation::kTagged);
      Label found_handler(this, &var_handler), stub_cache_miss(this);
      TryProbeStubCache(isolate()->load_stub_cache(), receiver, key,
                        &found_handler, &var_handler, &stub_cache_miss);
      Bind(&found_handler);
      { HandleLoadICHandlerCase(p, var_handler.value(), &slow); }

      Bind(&stub_cache_miss);
      {
        Comment("KeyedLoadGeneric_miss");
        TailCallRuntime(Runtime::kKeyedLoadIC_Miss, p->context, p->receiver,
                        p->name, p->slot, p->vector);
      }
    }
  }

  Bind(&if_property_dictionary);
  {
    Comment("dictionary property load");
    // We checked for LAST_CUSTOM_ELEMENTS_RECEIVER before, which rules out
    // seeing global objects here (which would need special handling).

    Variable var_name_index(this, MachineType::PointerRepresentation());
    Label dictionary_found(this, &var_name_index);
    NameDictionaryLookup<NameDictionary>(properties, key, &dictionary_found,
                                         &var_name_index, &slow);
    Bind(&dictionary_found);
    {
      LoadPropertyFromNameDictionary(properties, var_name_index.value(),
                                     &var_details, &var_value);
      Goto(&if_found_on_receiver);
    }
  }

  Bind(&if_found_on_receiver);
  {
    Node* value = CallGetterIfAccessor(var_value.value(), var_details.value(),
                                       p->context, receiver, &slow);
    IncrementCounter(isolate()->counters()->ic_keyed_load_generic_symbol(), 1);
    Return(value);
  }

  Bind(&slow);
  {
    Comment("KeyedLoadGeneric_slow");
    IncrementCounter(isolate()->counters()->ic_keyed_load_generic_slow(), 1);
    // TODO(jkummerow): Should we use the GetProperty TF stub instead?
    TailCallRuntime(Runtime::kKeyedGetProperty, p->context, p->receiver,
                    p->name);
  }
}

void CodeStubAssembler::HandleStoreFieldAndReturn(Node* handler_word,
                                                  Node* holder,
                                                  Representation representation,
                                                  Node* value, Node* transition,
                                                  Label* miss) {
  bool transition_to_field = transition != nullptr;
  Node* prepared_value = PrepareValueForWrite(value, representation, miss);

  if (transition_to_field) {
    Label storage_extended(this);
    GotoUnless(IsSetWord<StoreHandler::ExtendStorageBits>(handler_word),
               &storage_extended);
    Comment("[ Extend storage");
    ExtendPropertiesBackingStore(holder);
    Comment("] Extend storage");
    Goto(&storage_extended);

    Bind(&storage_extended);
  }

  Node* offset = DecodeWord<StoreHandler::FieldOffsetBits>(handler_word);
  Label if_inobject(this), if_out_of_object(this);
  Branch(IsSetWord<StoreHandler::IsInobjectBits>(handler_word), &if_inobject,
         &if_out_of_object);

  Bind(&if_inobject);
  {
    StoreNamedField(holder, offset, true, representation, prepared_value,
                    transition_to_field);
    if (transition_to_field) {
      StoreObjectField(holder, JSObject::kMapOffset, transition);
    }
    Return(value);
  }

  Bind(&if_out_of_object);
  {
    StoreNamedField(holder, offset, false, representation, prepared_value,
                    transition_to_field);
    if (transition_to_field) {
      StoreObjectField(holder, JSObject::kMapOffset, transition);
    }
    Return(value);
  }
}

void CodeStubAssembler::HandleStoreICSmiHandlerCase(Node* handler_word,
                                                    Node* holder, Node* value,
                                                    Node* transition,
                                                    Label* miss) {
  Comment(transition ? "transitioning field store" : "field store");

#ifdef DEBUG
  Node* handler_kind = DecodeWord<StoreHandler::KindBits>(handler_word);
  if (transition) {
    CSA_ASSERT(
        this,
        WordOr(WordEqual(handler_kind,
                         IntPtrConstant(StoreHandler::kTransitionToField)),
               WordEqual(handler_kind,
                         IntPtrConstant(StoreHandler::kTransitionToConstant))));
  } else {
    CSA_ASSERT(this, WordEqual(handler_kind,
                               IntPtrConstant(StoreHandler::kStoreField)));
  }
#endif

  Node* field_representation =
      DecodeWord<StoreHandler::FieldRepresentationBits>(handler_word);

  Label if_smi_field(this), if_double_field(this), if_heap_object_field(this),
      if_tagged_field(this);

  GotoIf(WordEqual(field_representation, IntPtrConstant(StoreHandler::kTagged)),
         &if_tagged_field);
  GotoIf(WordEqual(field_representation,
                   IntPtrConstant(StoreHandler::kHeapObject)),
         &if_heap_object_field);
  GotoIf(WordEqual(field_representation, IntPtrConstant(StoreHandler::kDouble)),
         &if_double_field);
  CSA_ASSERT(this, WordEqual(field_representation,
                             IntPtrConstant(StoreHandler::kSmi)));
  Goto(&if_smi_field);

  Bind(&if_tagged_field);
  {
    Comment("store tagged field");
    HandleStoreFieldAndReturn(handler_word, holder, Representation::Tagged(),
                              value, transition, miss);
  }

  Bind(&if_double_field);
  {
    Comment("store double field");
    HandleStoreFieldAndReturn(handler_word, holder, Representation::Double(),
                              value, transition, miss);
  }

  Bind(&if_heap_object_field);
  {
    Comment("store heap object field");
    // Generate full field type check here and then store value as Tagged.
    Node* prepared_value =
        PrepareValueForWrite(value, Representation::HeapObject(), miss);
    Node* value_index_in_descriptor =
        DecodeWord<StoreHandler::DescriptorValueIndexBits>(handler_word);
    Node* descriptors =
        LoadMapDescriptors(transition ? transition : LoadMap(holder));
    Node* maybe_field_type = LoadFixedArrayElement(
        descriptors, value_index_in_descriptor, 0, INTPTR_PARAMETERS);
    Label do_store(this);
    GotoIf(TaggedIsSmi(maybe_field_type), &do_store);
    // Check that value type matches the field type.
    {
      Node* field_type = LoadWeakCellValue(maybe_field_type, miss);
      Branch(WordEqual(LoadMap(prepared_value), field_type), &do_store, miss);
    }
    Bind(&do_store);
    HandleStoreFieldAndReturn(handler_word, holder, Representation::Tagged(),
                              prepared_value, transition, miss);
  }

  Bind(&if_smi_field);
  {
    Comment("store smi field");
    HandleStoreFieldAndReturn(handler_word, holder, Representation::Smi(),
                              value, transition, miss);
  }
}

void CodeStubAssembler::HandleStoreICHandlerCase(const StoreICParameters* p,
                                                 Node* handler, Label* miss) {
  Label if_smi_handler(this);
  Label try_proto_handler(this), call_handler(this);

  Branch(TaggedIsSmi(handler), &if_smi_handler, &try_proto_handler);

  // |handler| is a Smi, encoding what to do. See SmiHandler methods
  // for the encoding format.
  Bind(&if_smi_handler);
  {
    Node* holder = p->receiver;
    Node* handler_word = SmiUntag(handler);

    // Handle non-transitioning field stores.
    HandleStoreICSmiHandlerCase(handler_word, holder, p->value, nullptr, miss);
  }

  Bind(&try_proto_handler);
  {
    GotoIf(IsCodeMap(LoadMap(handler)), &call_handler);
    HandleStoreICProtoHandler(p, handler, miss);
  }

  // |handler| is a heap object. Must be code, call it.
  Bind(&call_handler);
  {
    StoreWithVectorDescriptor descriptor(isolate());
    TailCallStub(descriptor, handler, p->context, p->receiver, p->name,
                 p->value, p->slot, p->vector);
  }
}

void CodeStubAssembler::HandleStoreICProtoHandler(const StoreICParameters* p,
                                                  Node* handler, Label* miss) {
  // IC dispatchers rely on these assumptions to be held.
  STATIC_ASSERT(FixedArray::kLengthOffset ==
                StoreHandler::kTransitionCellOffset);
  DCHECK_EQ(FixedArray::OffsetOfElementAt(StoreHandler::kSmiHandlerIndex),
            StoreHandler::kSmiHandlerOffset);
  DCHECK_EQ(FixedArray::OffsetOfElementAt(StoreHandler::kValidityCellIndex),
            StoreHandler::kValidityCellOffset);

  // Both FixedArray and Tuple3 handlers have validity cell at the same offset.
  Label validity_cell_check_done(this);
  Node* validity_cell =
      LoadObjectField(handler, StoreHandler::kValidityCellOffset);
  GotoIf(WordEqual(validity_cell, IntPtrConstant(0)),
         &validity_cell_check_done);
  Node* cell_value = LoadObjectField(validity_cell, Cell::kValueOffset);
  GotoIf(WordNotEqual(cell_value,
                      SmiConstant(Smi::FromInt(Map::kPrototypeChainValid))),
         miss);
  Goto(&validity_cell_check_done);

  Bind(&validity_cell_check_done);
  Node* smi_handler = LoadObjectField(handler, StoreHandler::kSmiHandlerOffset);
  CSA_ASSERT(this, TaggedIsSmi(smi_handler));

  Node* maybe_transition_cell =
      LoadObjectField(handler, StoreHandler::kTransitionCellOffset);
  Label array_handler(this), tuple_handler(this);
  Branch(TaggedIsSmi(maybe_transition_cell), &array_handler, &tuple_handler);

  Variable var_transition(this, MachineRepresentation::kTagged);
  Label if_transition(this), if_transition_to_constant(this);
  Bind(&tuple_handler);
  {
    Node* transition = LoadWeakCellValue(maybe_transition_cell, miss);
    var_transition.Bind(transition);
    Goto(&if_transition);
  }

  Bind(&array_handler);
  {
    Node* length = SmiUntag(maybe_transition_cell);
    BuildFastLoop(MachineType::PointerRepresentation(),
                  IntPtrConstant(StoreHandler::kFirstPrototypeIndex), length,
                  [this, p, handler, miss](CodeStubAssembler*, Node* current) {
                    Node* prototype_cell = LoadFixedArrayElement(
                        handler, current, 0, INTPTR_PARAMETERS);
                    CheckPrototype(prototype_cell, p->name, miss);
                  },
                  1, IndexAdvanceMode::kPost);

    Node* maybe_transition_cell = LoadFixedArrayElement(
        handler, IntPtrConstant(StoreHandler::kTransitionCellIndex), 0,
        INTPTR_PARAMETERS);
    Node* transition = LoadWeakCellValue(maybe_transition_cell, miss);
    var_transition.Bind(transition);
    Goto(&if_transition);
  }

  Bind(&if_transition);
  {
    Node* holder = p->receiver;
    Node* transition = var_transition.value();
    Node* handler_word = SmiUntag(smi_handler);

    GotoIf(IsSetWord32<Map::Deprecated>(LoadMapBitField3(transition)), miss);

    Node* handler_kind = DecodeWord<StoreHandler::KindBits>(handler_word);
    GotoIf(WordEqual(handler_kind,
                     IntPtrConstant(StoreHandler::kTransitionToConstant)),
           &if_transition_to_constant);

    // Handle transitioning field stores.
    HandleStoreICSmiHandlerCase(handler_word, holder, p->value, transition,
                                miss);

    Bind(&if_transition_to_constant);
    {
      // Check that constant matches value.
      Node* value_index_in_descriptor =
          DecodeWord<StoreHandler::DescriptorValueIndexBits>(handler_word);
      Node* descriptors = LoadMapDescriptors(transition);
      Node* constant = LoadFixedArrayElement(
          descriptors, value_index_in_descriptor, 0, INTPTR_PARAMETERS);
      GotoIf(WordNotEqual(p->value, constant), miss);

      StoreObjectField(p->receiver, JSObject::kMapOffset, transition);
      Return(p->value);
    }
  }
}

void CodeStubAssembler::StoreIC(const StoreICParameters* p) {
  Variable var_handler(this, MachineRepresentation::kTagged);
  // TODO(ishell): defer blocks when it works.
  Label if_handler(this, &var_handler), try_polymorphic(this),
      try_megamorphic(this /*, Label::kDeferred*/),
      miss(this /*, Label::kDeferred*/);

  Node* receiver_map = LoadReceiverMap(p->receiver);

  // Check monomorphic case.
  Node* feedback =
      TryMonomorphicCase(p->slot, p->vector, receiver_map, &if_handler,
                         &var_handler, &try_polymorphic);
  Bind(&if_handler);
  {
    Comment("StoreIC_if_handler");
    HandleStoreICHandlerCase(p, var_handler.value(), &miss);
  }

  Bind(&try_polymorphic);
  {
    // Check polymorphic case.
    Comment("StoreIC_try_polymorphic");
    GotoUnless(
        WordEqual(LoadMap(feedback), LoadRoot(Heap::kFixedArrayMapRootIndex)),
        &try_megamorphic);
    HandlePolymorphicCase(receiver_map, feedback, &if_handler, &var_handler,
                          &miss, 2);
  }

  Bind(&try_megamorphic);
  {
    // Check megamorphic case.
    GotoUnless(
        WordEqual(feedback, LoadRoot(Heap::kmegamorphic_symbolRootIndex)),
        &miss);

    TryProbeStubCache(isolate()->store_stub_cache(), p->receiver, p->name,
                      &if_handler, &var_handler, &miss);
  }
  Bind(&miss);
  {
    TailCallRuntime(Runtime::kStoreIC_Miss, p->context, p->value, p->slot,
                    p->vector, p->receiver, p->name);
  }
}

void CodeStubAssembler::KeyedStoreIC(const StoreICParameters* p,
                                     LanguageMode language_mode) {
  Variable var_handler(this, MachineRepresentation::kTagged);
  // This is to make |miss| label see the var_handler bound on all paths.
  var_handler.Bind(IntPtrConstant(0));

  // TODO(ishell): defer blocks when it works.
  Label if_handler(this, &var_handler), try_polymorphic(this),
      try_megamorphic(this /*, Label::kDeferred*/),
      try_polymorphic_name(this /*, Label::kDeferred*/),
      miss(this /*, Label::kDeferred*/);

  Node* receiver_map = LoadReceiverMap(p->receiver);

  // Check monomorphic case.
  Node* feedback =
      TryMonomorphicCase(p->slot, p->vector, receiver_map, &if_handler,
                         &var_handler, &try_polymorphic);
  Bind(&if_handler);
  {
    Comment("KeyedStoreIC_if_handler");
    HandleStoreICHandlerCase(p, var_handler.value(), &miss);
  }

  Bind(&try_polymorphic);
  {
    // CheckPolymorphic case.
    Comment("KeyedStoreIC_try_polymorphic");
    GotoUnless(
        WordEqual(LoadMap(feedback), LoadRoot(Heap::kFixedArrayMapRootIndex)),
        &try_megamorphic);
    Label if_transition_handler(this);
    Variable var_transition_map_cell(this, MachineRepresentation::kTagged);
    HandleKeyedStorePolymorphicCase(receiver_map, feedback, &if_handler,
                                    &var_handler, &if_transition_handler,
                                    &var_transition_map_cell, &miss);
    Bind(&if_transition_handler);
    Comment("KeyedStoreIC_polymorphic_transition");
    Node* transition_map =
        LoadWeakCellValue(var_transition_map_cell.value(), &miss);
    StoreTransitionDescriptor descriptor(isolate());
    TailCallStub(descriptor, var_handler.value(), p->context, p->receiver,
                 p->name, transition_map, p->value, p->slot, p->vector);
  }

  Bind(&try_megamorphic);
  {
    // Check megamorphic case.
    Comment("KeyedStoreIC_try_megamorphic");
    GotoUnless(
        WordEqual(feedback, LoadRoot(Heap::kmegamorphic_symbolRootIndex)),
        &try_polymorphic_name);
    TailCallStub(
        CodeFactory::KeyedStoreIC_Megamorphic(isolate(), language_mode),
        p->context, p->receiver, p->name, p->value, p->slot, p->vector);
  }

  Bind(&try_polymorphic_name);
  {
    // We might have a name in feedback, and a fixed array in the next slot.
    Comment("KeyedStoreIC_try_polymorphic_name");
    GotoUnless(WordEqual(feedback, p->name), &miss);
    // If the name comparison succeeded, we know we have a FixedArray with
    // at least one map/handler pair.
    Node* offset = ElementOffsetFromIndex(
        p->slot, FAST_HOLEY_ELEMENTS, SMI_PARAMETERS,
        FixedArray::kHeaderSize + kPointerSize - kHeapObjectTag);
    Node* array = Load(MachineType::AnyTagged(), p->vector, offset);
    HandlePolymorphicCase(receiver_map, array, &if_handler, &var_handler, &miss,
                          1);
  }

  Bind(&miss);
  {
    Comment("KeyedStoreIC_miss");
    TailCallRuntime(Runtime::kKeyedStoreIC_Miss, p->context, p->value, p->slot,
                    p->vector, p->receiver, p->name);
  }
}

void CodeStubAssembler::LoadGlobalIC(const LoadICParameters* p) {
  Label try_handler(this), miss(this);
  Node* weak_cell =
      LoadFixedArrayElement(p->vector, p->slot, 0, SMI_PARAMETERS);
  CSA_ASSERT(this, HasInstanceType(weak_cell, WEAK_CELL_TYPE));

  // Load value or try handler case if the {weak_cell} is cleared.
  Node* property_cell = LoadWeakCellValue(weak_cell, &try_handler);
  CSA_ASSERT(this, HasInstanceType(property_cell, PROPERTY_CELL_TYPE));

  Node* value = LoadObjectField(property_cell, PropertyCell::kValueOffset);
  GotoIf(WordEqual(value, TheHoleConstant()), &miss);
  Return(value);

  Bind(&try_handler);
  {
    Node* handler =
        LoadFixedArrayElement(p->vector, p->slot, kPointerSize, SMI_PARAMETERS);
    GotoIf(WordEqual(handler, LoadRoot(Heap::kuninitialized_symbolRootIndex)),
           &miss);

    // In this case {handler} must be a Code object.
    CSA_ASSERT(this, HasInstanceType(handler, CODE_TYPE));
    LoadWithVectorDescriptor descriptor(isolate());
    Node* native_context = LoadNativeContext(p->context);
    Node* receiver =
        LoadContextElement(native_context, Context::EXTENSION_INDEX);
    Node* fake_name = IntPtrConstant(0);
    TailCallStub(descriptor, handler, p->context, receiver, fake_name, p->slot,
                 p->vector);
  }
  Bind(&miss);
  {
    TailCallRuntime(Runtime::kLoadGlobalIC_Miss, p->context, p->slot,
                    p->vector);
  }
}

void CodeStubAssembler::ExtendPropertiesBackingStore(compiler::Node* object) {
  Node* properties = LoadProperties(object);
  Node* length = LoadFixedArrayBaseLength(properties);

  ParameterMode mode = OptimalParameterMode();
  length = UntagParameter(length, mode);

  Node* delta = IntPtrOrSmiConstant(JSObject::kFieldsAdded, mode);
  Node* new_capacity = IntPtrAdd(length, delta);

  // Grow properties array.
  ElementsKind kind = FAST_ELEMENTS;
  DCHECK(kMaxNumberOfDescriptors + JSObject::kFieldsAdded <
         FixedArrayBase::GetMaxLengthForNewSpaceAllocation(kind));
  // The size of a new properties backing store is guaranteed to be small
  // enough that the new backing store will be allocated in new space.
  CSA_ASSERT(this, UintPtrLessThan(new_capacity,
                                   IntPtrConstant(kMaxNumberOfDescriptors +
                                                  JSObject::kFieldsAdded)));

  Node* new_properties = AllocateFixedArray(kind, new_capacity, mode);

  FillFixedArrayWithValue(kind, new_properties, length, new_capacity,
                          Heap::kUndefinedValueRootIndex, mode);

  // |new_properties| is guaranteed to be in new space, so we can skip
  // the write barrier.
  CopyFixedArrayElements(kind, properties, new_properties, length,
                         SKIP_WRITE_BARRIER, mode);

  StoreObjectField(object, JSObject::kPropertiesOffset, new_properties);
}

Node* CodeStubAssembler::PrepareValueForWrite(Node* value,
                                              Representation representation,
                                              Label* bailout) {
  if (representation.IsDouble()) {
    value = TryTaggedToFloat64(value, bailout);
  } else if (representation.IsHeapObject()) {
    // Field type is checked by the handler, here we only check if the value
    // is a heap object.
    GotoIf(TaggedIsSmi(value), bailout);
  } else if (representation.IsSmi()) {
    GotoUnless(TaggedIsSmi(value), bailout);
  } else {
    DCHECK(representation.IsTagged());
  }
  return value;
}

void CodeStubAssembler::StoreNamedField(Node* object, FieldIndex index,
                                        Representation representation,
                                        Node* value, bool transition_to_field) {
  DCHECK_EQ(index.is_double(), representation.IsDouble());

  StoreNamedField(object, IntPtrConstant(index.offset()), index.is_inobject(),
                  representation, value, transition_to_field);
}

void CodeStubAssembler::StoreNamedField(Node* object, Node* offset,
                                        bool is_inobject,
                                        Representation representation,
                                        Node* value, bool transition_to_field) {
  bool store_value_as_double = representation.IsDouble();
  Node* property_storage = object;
  if (!is_inobject) {
    property_storage = LoadProperties(object);
  }

  if (representation.IsDouble()) {
    if (!FLAG_unbox_double_fields || !is_inobject) {
      if (transition_to_field) {
        Node* heap_number = AllocateHeapNumberWithValue(value, MUTABLE);
        // Store the new mutable heap number into the object.
        value = heap_number;
        store_value_as_double = false;
      } else {
        // Load the heap number.
        property_storage = LoadObjectField(property_storage, offset);
        // Store the double value into it.
        offset = IntPtrConstant(HeapNumber::kValueOffset);
      }
    }
  }

  if (store_value_as_double) {
    StoreObjectFieldNoWriteBarrier(property_storage, offset, value,
                                   MachineRepresentation::kFloat64);
  } else if (representation.IsSmi()) {
    StoreObjectFieldNoWriteBarrier(property_storage, offset, value);
  } else {
    StoreObjectField(property_storage, offset, value);
  }
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

  GotoUnless(TaggedIsSmi(key), bailout);
  key = SmiUntag(key);
  GotoIf(IntPtrLessThan(key, IntPtrConstant(0)), bailout);

  Node* elements = LoadElements(receiver);
  Node* elements_length = LoadAndUntagFixedArrayBaseLength(elements);

  Variable var_result(this, MachineRepresentation::kTagged);
  if (!is_load) {
    var_result.Bind(value);
  }
  Label if_mapped(this), if_unmapped(this), end(this, &var_result);
  Node* intptr_two = IntPtrConstant(2);
  Node* adjusted_length = IntPtrSub(elements_length, intptr_two);

  GotoIf(UintPtrGreaterThanOrEqual(key, adjusted_length), &if_unmapped);

  Node* mapped_index = LoadFixedArrayElement(
      elements, IntPtrAdd(key, intptr_two), 0, INTPTR_PARAMETERS);
  Branch(WordEqual(mapped_index, TheHoleConstant()), &if_unmapped, &if_mapped);

  Bind(&if_mapped);
  {
    CSA_ASSERT(this, TaggedIsSmi(mapped_index));
    mapped_index = SmiUntag(mapped_index);
    Node* the_context = LoadFixedArrayElement(elements, IntPtrConstant(0), 0,
                                              INTPTR_PARAMETERS);
    // Assert that we can use LoadFixedArrayElement/StoreFixedArrayElement
    // methods for accessing Context.
    STATIC_ASSERT(Context::kHeaderSize == FixedArray::kHeaderSize);
    DCHECK_EQ(Context::SlotOffset(0) + kHeapObjectTag,
              FixedArray::OffsetOfElementAt(0));
    if (is_load) {
      Node* result = LoadFixedArrayElement(the_context, mapped_index, 0,
                                           INTPTR_PARAMETERS);
      CSA_ASSERT(this, WordNotEqual(result, TheHoleConstant()));
      var_result.Bind(result);
    } else {
      StoreFixedArrayElement(the_context, mapped_index, value,
                             UPDATE_WRITE_BARRIER, INTPTR_PARAMETERS);
    }
    Goto(&end);
  }

  Bind(&if_unmapped);
  {
    Node* backing_store = LoadFixedArrayElement(elements, IntPtrConstant(1), 0,
                                                INTPTR_PARAMETERS);
    GotoIf(WordNotEqual(LoadMap(backing_store), FixedArrayMapConstant()),
           bailout);

    Node* backing_store_length =
        LoadAndUntagFixedArrayBaseLength(backing_store);
    GotoIf(UintPtrGreaterThanOrEqual(key, backing_store_length), bailout);

    // The key falls into unmapped range.
    if (is_load) {
      Node* result =
          LoadFixedArrayElement(backing_store, key, 0, INTPTR_PARAMETERS);
      GotoIf(WordEqual(result, TheHoleConstant()), bailout);
      var_result.Bind(result);
    } else {
      StoreFixedArrayElement(backing_store, key, value, UPDATE_WRITE_BARRIER,
                             INTPTR_PARAMETERS);
    }
    Goto(&end);
  }

  Bind(&end);
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
      return MachineRepresentation::kNone;
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
      IsFastSmiElementsKind(kind) ? SKIP_WRITE_BARRIER : UPDATE_WRITE_BARRIER;
  if (IsFastDoubleElementsKind(kind)) {
    // Make sure we do not store signalling NaNs into double arrays.
    value = Float64SilenceNaN(value);
    StoreFixedDoubleArrayElement(elements, index, value, mode);
  } else {
    StoreFixedArrayElement(elements, index, value, barrier_mode, mode);
  }
}

Node* CodeStubAssembler::Int32ToUint8Clamped(Node* int32_value) {
  Label done(this);
  Node* int32_zero = Int32Constant(0);
  Node* int32_255 = Int32Constant(255);
  Variable var_value(this, MachineRepresentation::kWord32);
  var_value.Bind(int32_value);
  GotoIf(Uint32LessThanOrEqual(int32_value, int32_255), &done);
  var_value.Bind(int32_zero);
  GotoIf(Int32LessThan(int32_value, int32_zero), &done);
  var_value.Bind(int32_255);
  Goto(&done);
  Bind(&done);
  return var_value.value();
}

Node* CodeStubAssembler::Float64ToUint8Clamped(Node* float64_value) {
  Label done(this);
  Variable var_value(this, MachineRepresentation::kWord32);
  var_value.Bind(Int32Constant(0));
  GotoIf(Float64LessThanOrEqual(float64_value, Float64Constant(0.0)), &done);
  var_value.Bind(Int32Constant(255));
  GotoIf(Float64LessThanOrEqual(Float64Constant(255.0), float64_value), &done);
  {
    Node* rounded_value = Float64RoundToEven(float64_value);
    var_value.Bind(TruncateFloat64ToWord32(rounded_value));
    Goto(&done);
  }
  Bind(&done);
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
      return nullptr;
  }

  Variable var_result(this, rep);
  Label done(this, &var_result), if_smi(this);
  GotoIf(TaggedIsSmi(input), &if_smi);
  // Try to convert a heap number to a Smi.
  GotoUnless(IsHeapNumberMap(LoadMap(input)), bailout);
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

  Bind(&if_smi);
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

  Bind(&done);
  return var_result.value();
}

void CodeStubAssembler::EmitElementStore(Node* object, Node* key, Node* value,
                                         bool is_jsarray,
                                         ElementsKind elements_kind,
                                         KeyedAccessStoreMode store_mode,
                                         Label* bailout) {
  Node* elements = LoadElements(object);
  if (IsFastSmiOrObjectElementsKind(elements_kind) &&
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
    Node* bitfield = LoadObjectField(buffer, JSArrayBuffer::kBitFieldOffset,
                                     MachineType::Uint32());
    Node* neutered_bit =
        Word32And(bitfield, Int32Constant(JSArrayBuffer::WasNeutered::kMask));
    GotoUnless(Word32Equal(neutered_bit, Int32Constant(0)), bailout);

    // Bounds check.
    Node* length = UntagParameter(
        LoadObjectField(object, JSTypedArray::kLengthOffset), parameter_mode);

    if (store_mode == STORE_NO_TRANSITION_IGNORE_OUT_OF_BOUNDS) {
      // Skip the store if we write beyond the length.
      GotoUnless(IntPtrLessThan(key, length), &done);
      // ... but bailout if the key is negative.
    } else {
      DCHECK_EQ(STANDARD_STORE, store_mode);
    }
    GotoUnless(UintPtrLessThan(key, length), bailout);

    // Backing store = external_pointer + base_pointer.
    Node* external_pointer =
        LoadObjectField(elements, FixedTypedArrayBase::kExternalPointerOffset,
                        MachineType::Pointer());
    Node* base_pointer =
        LoadObjectField(elements, FixedTypedArrayBase::kBasePointerOffset);
    Node* backing_store = IntPtrAdd(external_pointer, base_pointer);
    StoreElement(backing_store, elements_kind, key, value, parameter_mode);
    Goto(&done);

    Bind(&done);
    return;
  }
  DCHECK(IsFastSmiOrObjectElementsKind(elements_kind) ||
         IsFastDoubleElementsKind(elements_kind));

  Node* length = is_jsarray ? LoadObjectField(object, JSArray::kLengthOffset)
                            : LoadFixedArrayBaseLength(elements);
  length = UntagParameter(length, parameter_mode);

  // In case value is stored into a fast smi array, assure that the value is
  // a smi before manipulating the backing store. Otherwise the backing store
  // may be left in an invalid state.
  if (IsFastSmiElementsKind(elements_kind)) {
    GotoUnless(TaggedIsSmi(value), bailout);
  } else if (IsFastDoubleElementsKind(elements_kind)) {
    value = TryTaggedToFloat64(value, bailout);
  }

  if (IsGrowStoreMode(store_mode)) {
    elements = CheckForCapacityGrow(object, elements, elements_kind, length,
                                    key, parameter_mode, is_jsarray, bailout);
  } else {
    GotoUnless(UintPtrLessThan(key, length), bailout);

    if ((store_mode == STORE_NO_TRANSITION_HANDLE_COW) &&
        IsFastSmiOrObjectElementsKind(elements_kind)) {
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
  Variable checked_elements(this, MachineRepresentation::kTagged);
  Label grow_case(this), no_grow_case(this), done(this);

  Node* condition;
  if (IsHoleyElementsKind(kind)) {
    condition = UintPtrGreaterThanOrEqual(key, length);
  } else {
    condition = WordEqual(key, length);
  }
  Branch(condition, &grow_case, &no_grow_case);

  Bind(&grow_case);
  {
    Node* current_capacity =
        UntagParameter(LoadFixedArrayBaseLength(elements), mode);

    checked_elements.Bind(elements);

    Label fits_capacity(this);
    GotoIf(UintPtrLessThan(key, current_capacity), &fits_capacity);
    {
      Node* new_elements = TryGrowElementsCapacity(
          object, elements, kind, key, current_capacity, mode, bailout);

      checked_elements.Bind(new_elements);
      Goto(&fits_capacity);
    }
    Bind(&fits_capacity);

    if (is_js_array) {
      Node* new_length = IntPtrAdd(key, IntPtrOrSmiConstant(1, mode));
      StoreObjectFieldNoWriteBarrier(object, JSArray::kLengthOffset,
                                     TagParameter(new_length, mode));
    }
    Goto(&done);
  }

  Bind(&no_grow_case);
  {
    GotoUnless(UintPtrLessThan(key, length), bailout);
    checked_elements.Bind(elements);
    Goto(&done);
  }

  Bind(&done);
  return checked_elements.value();
}

Node* CodeStubAssembler::CopyElementsOnWrite(Node* object, Node* elements,
                                             ElementsKind kind, Node* length,
                                             ParameterMode mode,
                                             Label* bailout) {
  Variable new_elements_var(this, MachineRepresentation::kTagged);
  Label done(this);

  new_elements_var.Bind(elements);
  GotoUnless(
      WordEqual(LoadMap(elements), LoadRoot(Heap::kFixedCOWArrayMapRootIndex)),
      &done);
  {
    Node* capacity = UntagParameter(LoadFixedArrayBaseLength(elements), mode);
    Node* new_elements = GrowElementsCapacity(object, elements, kind, kind,
                                              length, capacity, mode, bailout);

    new_elements_var.Bind(new_elements);
    Goto(&done);
  }

  Bind(&done);
  return new_elements_var.value();
}

void CodeStubAssembler::TransitionElementsKind(
    compiler::Node* object, compiler::Node* map, ElementsKind from_kind,
    ElementsKind to_kind, bool is_jsarray, Label* bailout) {
  DCHECK(!IsFastHoleyElementsKind(from_kind) ||
         IsFastHoleyElementsKind(to_kind));
  if (AllocationSite::GetMode(from_kind, to_kind) == TRACK_ALLOCATION_SITE) {
    TrapAllocationMemento(object, bailout);
  }

  if (!IsSimpleMapChangeTransition(from_kind, to_kind)) {
    Comment("Non-simple map transition");
    Node* elements = LoadElements(object);

    Node* empty_fixed_array =
        HeapConstant(isolate()->factory()->empty_fixed_array());

    Label done(this);
    GotoIf(WordEqual(elements, empty_fixed_array), &done);

    // TODO(ishell): Use OptimalParameterMode().
    ParameterMode mode = INTPTR_PARAMETERS;
    Node* elements_length = SmiUntag(LoadFixedArrayBaseLength(elements));
    Node* array_length =
        is_jsarray ? SmiUntag(LoadObjectField(object, JSArray::kLengthOffset))
                   : elements_length;

    GrowElementsCapacity(object, elements, from_kind, to_kind, array_length,
                         elements_length, mode, bailout);
    Goto(&done);
    Bind(&done);
  }

  StoreObjectField(object, JSObject::kMapOffset, map);
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
  Node* object_page = PageFromAddress(object);
  {
    Node* page_flags = Load(MachineType::IntPtr(), object_page,
                            IntPtrConstant(Page::kFlagsOffset));
    GotoIf(WordEqual(WordAnd(page_flags,
                             IntPtrConstant(MemoryChunk::kIsInNewSpaceMask)),
                     IntPtrConstant(0)),
           &no_memento_found);
  }

  Node* memento_last_word = IntPtrAdd(
      object, IntPtrConstant(kMementoLastWordOffset - kHeapObjectTag));
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
  Bind(&top_check);
  {
    Branch(UintPtrGreaterThanOrEqual(memento_last_word, new_space_top),
           &no_memento_found, &map_check);
  }

  // Memento map check.
  Bind(&map_check);
  {
    Node* memento_map = LoadObjectField(object, kMementoMapOffset);
    Branch(
        WordEqual(memento_map, LoadRoot(Heap::kAllocationMementoMapRootIndex)),
        memento_found, &no_memento_found);
  }
  Bind(&no_memento_found);
  Comment("] TrapAllocationMemento");
}

Node* CodeStubAssembler::PageFromAddress(Node* address) {
  return WordAnd(address, IntPtrConstant(~Page::kPageAlignmentMask));
}

Node* CodeStubAssembler::EnumLength(Node* map) {
  CSA_ASSERT(this, IsMap(map));
  Node* bitfield_3 = LoadMapBitField3(map);
  Node* enum_length = DecodeWordFromWord32<Map::EnumLengthBits>(bitfield_3);
  return SmiTag(enum_length);
}

void CodeStubAssembler::CheckEnumCache(Node* receiver, Label* use_cache,
                                       Label* use_runtime) {
  Variable current_js_object(this, MachineRepresentation::kTagged);
  current_js_object.Bind(receiver);

  Variable current_map(this, MachineRepresentation::kTagged);
  current_map.Bind(LoadMap(current_js_object.value()));

  // These variables are updated in the loop below.
  Variable* loop_vars[2] = {&current_js_object, &current_map};
  Label loop(this, 2, loop_vars), next(this);

  // Check if the enum length field is properly initialized, indicating that
  // there is an enum cache.
  {
    Node* invalid_enum_cache_sentinel =
        SmiConstant(Smi::FromInt(kInvalidEnumCacheSentinel));
    Node* enum_length = EnumLength(current_map.value());
    Branch(WordEqual(enum_length, invalid_enum_cache_sentinel), use_runtime,
           &loop);
  }

  // Check that there are no elements. |current_js_object| contains
  // the current JS object we've reached through the prototype chain.
  Bind(&loop);
  {
    Label if_elements(this), if_no_elements(this);
    Node* elements = LoadElements(current_js_object.value());
    Node* empty_fixed_array = LoadRoot(Heap::kEmptyFixedArrayRootIndex);
    // Check that there are no elements.
    Branch(WordEqual(elements, empty_fixed_array), &if_no_elements,
           &if_elements);
    Bind(&if_elements);
    {
      // Second chance, the object may be using the empty slow element
      // dictionary.
      Node* slow_empty_dictionary =
          LoadRoot(Heap::kEmptySlowElementDictionaryRootIndex);
      Branch(WordNotEqual(elements, slow_empty_dictionary), use_runtime,
             &if_no_elements);
    }

    Bind(&if_no_elements);
    {
      // Update map prototype.
      current_js_object.Bind(LoadMapPrototype(current_map.value()));
      Branch(WordEqual(current_js_object.value(), NullConstant()), use_cache,
             &next);
    }
  }

  Bind(&next);
  {
    // For all objects but the receiver, check that the cache is empty.
    current_map.Bind(LoadMap(current_js_object.value()));
    Node* enum_length = EnumLength(current_map.value());
    Node* zero_constant = SmiConstant(Smi::kZero);
    Branch(WordEqual(enum_length, zero_constant), &loop, use_runtime);
  }
}

Node* CodeStubAssembler::CreateAllocationSiteInFeedbackVector(
    Node* feedback_vector, Node* slot) {
  Node* size = IntPtrConstant(AllocationSite::kSize);
  Node* site = Allocate(size, CodeStubAssembler::kPretenured);

  // Store the map
  StoreObjectFieldRoot(site, AllocationSite::kMapOffset,
                       Heap::kAllocationSiteMapRootIndex);
  Node* kind = SmiConstant(Smi::FromInt(GetInitialFastElementsKind()));
  StoreObjectFieldNoWriteBarrier(site, AllocationSite::kTransitionInfoOffset,
                                 kind);

  // Unlike literals, constructed arrays don't have nested sites
  Node* zero = IntPtrConstant(0);
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

  StoreFixedArrayElement(feedback_vector, slot, site, UPDATE_WRITE_BARRIER,
                         CodeStubAssembler::SMI_PARAMETERS);
  return site;
}

Node* CodeStubAssembler::CreateWeakCellInFeedbackVector(Node* feedback_vector,
                                                        Node* slot,
                                                        Node* value) {
  Node* size = IntPtrConstant(WeakCell::kSize);
  Node* cell = Allocate(size, CodeStubAssembler::kPretenured);

  // Initialize the WeakCell.
  StoreObjectFieldRoot(cell, WeakCell::kMapOffset, Heap::kWeakCellMapRootIndex);
  StoreObjectField(cell, WeakCell::kValueOffset, value);
  StoreObjectFieldRoot(cell, WeakCell::kNextOffset,
                       Heap::kTheHoleValueRootIndex);

  // Store the WeakCell in the feedback vector.
  StoreFixedArrayElement(feedback_vector, slot, cell, UPDATE_WRITE_BARRIER,
                         CodeStubAssembler::SMI_PARAMETERS);
  return cell;
}

void CodeStubAssembler::BuildFastLoop(
    const CodeStubAssembler::VariableList& vars,
    MachineRepresentation index_rep, Node* start_index, Node* end_index,
    std::function<void(CodeStubAssembler* assembler, Node* index)> body,
    int increment, IndexAdvanceMode mode) {
  Variable var(this, index_rep);
  VariableList vars_copy(vars, zone());
  vars_copy.Add(&var, zone());
  var.Bind(start_index);
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
  Bind(&loop);
  {
    if (mode == IndexAdvanceMode::kPre) {
      var.Bind(IntPtrAdd(var.value(), IntPtrConstant(increment)));
    }
    body(this, var.value());
    if (mode == IndexAdvanceMode::kPost) {
      var.Bind(IntPtrAdd(var.value(), IntPtrConstant(increment)));
    }
    Branch(WordNotEqual(var.value(), end_index), &loop, &after_loop);
  }
  Bind(&after_loop);
}

void CodeStubAssembler::BuildFastFixedArrayForEach(
    compiler::Node* fixed_array, ElementsKind kind,
    compiler::Node* first_element_inclusive,
    compiler::Node* last_element_exclusive,
    std::function<void(CodeStubAssembler* assembler,
                       compiler::Node* fixed_array, compiler::Node* offset)>
        body,
    ParameterMode mode, ForEachDirection direction) {
  STATIC_ASSERT(FixedArray::kHeaderSize == FixedDoubleArray::kHeaderSize);
  int32_t first_val;
  bool constant_first = ToInt32Constant(first_element_inclusive, first_val);
  int32_t last_val;
  bool constent_last = ToInt32Constant(last_element_exclusive, last_val);
  if (constant_first && constent_last) {
    int delta = last_val - first_val;
    DCHECK(delta >= 0);
    if (delta <= kElementLoopUnrollThreshold) {
      if (direction == ForEachDirection::kForward) {
        for (int i = first_val; i < last_val; ++i) {
          Node* index = IntPtrConstant(i);
          Node* offset =
              ElementOffsetFromIndex(index, kind, INTPTR_PARAMETERS,
                                     FixedArray::kHeaderSize - kHeapObjectTag);
          body(this, fixed_array, offset);
        }
      } else {
        for (int i = last_val - 1; i >= first_val; --i) {
          Node* index = IntPtrConstant(i);
          Node* offset =
              ElementOffsetFromIndex(index, kind, INTPTR_PARAMETERS,
                                     FixedArray::kHeaderSize - kHeapObjectTag);
          body(this, fixed_array, offset);
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

  int increment = IsFastDoubleElementsKind(kind) ? kDoubleSize : kPointerSize;
  BuildFastLoop(
      MachineType::PointerRepresentation(), start, limit,
      [fixed_array, body](CodeStubAssembler* assembler, Node* offset) {
        body(assembler, fixed_array, offset);
      },
      direction == ForEachDirection::kReverse ? -increment : increment,
      direction == ForEachDirection::kReverse ? IndexAdvanceMode::kPre
                                              : IndexAdvanceMode::kPost);
}

void CodeStubAssembler::BranchIfNumericRelationalComparison(
    RelationalComparisonMode mode, compiler::Node* lhs, compiler::Node* rhs,
    Label* if_true, Label* if_false) {
  typedef compiler::Node Node;

  Label end(this);
  Variable result(this, MachineRepresentation::kTagged);

  // Shared entry for floating point comparison.
  Label do_fcmp(this);
  Variable var_fcmp_lhs(this, MachineRepresentation::kFloat64),
      var_fcmp_rhs(this, MachineRepresentation::kFloat64);

  // Check if the {lhs} is a Smi or a HeapObject.
  Label if_lhsissmi(this), if_lhsisnotsmi(this);
  Branch(TaggedIsSmi(lhs), &if_lhsissmi, &if_lhsisnotsmi);

  Bind(&if_lhsissmi);
  {
    // Check if {rhs} is a Smi or a HeapObject.
    Label if_rhsissmi(this), if_rhsisnotsmi(this);
    Branch(TaggedIsSmi(rhs), &if_rhsissmi, &if_rhsisnotsmi);

    Bind(&if_rhsissmi);
    {
      // Both {lhs} and {rhs} are Smi, so just perform a fast Smi comparison.
      switch (mode) {
        case kLessThan:
          BranchIfSmiLessThan(lhs, rhs, if_true, if_false);
          break;
        case kLessThanOrEqual:
          BranchIfSmiLessThanOrEqual(lhs, rhs, if_true, if_false);
          break;
        case kGreaterThan:
          BranchIfSmiLessThan(rhs, lhs, if_true, if_false);
          break;
        case kGreaterThanOrEqual:
          BranchIfSmiLessThanOrEqual(rhs, lhs, if_true, if_false);
          break;
      }
    }

    Bind(&if_rhsisnotsmi);
    {
      CSA_ASSERT(this, WordEqual(LoadMap(rhs), HeapNumberMapConstant()));
      // Convert the {lhs} and {rhs} to floating point values, and
      // perform a floating point comparison.
      var_fcmp_lhs.Bind(SmiToFloat64(lhs));
      var_fcmp_rhs.Bind(LoadHeapNumberValue(rhs));
      Goto(&do_fcmp);
    }
  }

  Bind(&if_lhsisnotsmi);
  {
    CSA_ASSERT(this, WordEqual(LoadMap(lhs), HeapNumberMapConstant()));

    // Check if {rhs} is a Smi or a HeapObject.
    Label if_rhsissmi(this), if_rhsisnotsmi(this);
    Branch(TaggedIsSmi(rhs), &if_rhsissmi, &if_rhsisnotsmi);

    Bind(&if_rhsissmi);
    {
      // Convert the {lhs} and {rhs} to floating point values, and
      // perform a floating point comparison.
      var_fcmp_lhs.Bind(LoadHeapNumberValue(lhs));
      var_fcmp_rhs.Bind(SmiToFloat64(rhs));
      Goto(&do_fcmp);
    }

    Bind(&if_rhsisnotsmi);
    {
      CSA_ASSERT(this, WordEqual(LoadMap(rhs), HeapNumberMapConstant()));

      // Convert the {lhs} and {rhs} to floating point values, and
      // perform a floating point comparison.
      var_fcmp_lhs.Bind(LoadHeapNumberValue(lhs));
      var_fcmp_rhs.Bind(LoadHeapNumberValue(rhs));
      Goto(&do_fcmp);
    }
  }

  Bind(&do_fcmp);
  {
    // Load the {lhs} and {rhs} floating point values.
    Node* lhs = var_fcmp_lhs.value();
    Node* rhs = var_fcmp_rhs.value();

    // Perform a fast floating point comparison.
    switch (mode) {
      case kLessThan:
        Branch(Float64LessThan(lhs, rhs), if_true, if_false);
        break;
      case kLessThanOrEqual:
        Branch(Float64LessThanOrEqual(lhs, rhs), if_true, if_false);
        break;
      case kGreaterThan:
        Branch(Float64GreaterThan(lhs, rhs), if_true, if_false);
        break;
      case kGreaterThanOrEqual:
        Branch(Float64GreaterThanOrEqual(lhs, rhs), if_true, if_false);
        break;
    }
  }
}

void CodeStubAssembler::GotoUnlessNumberLessThan(compiler::Node* lhs,
                                                 compiler::Node* rhs,
                                                 Label* if_false) {
  Label if_true(this);
  BranchIfNumericRelationalComparison(kLessThan, lhs, rhs, &if_true, if_false);
  Bind(&if_true);
}

compiler::Node* CodeStubAssembler::RelationalComparison(
    RelationalComparisonMode mode, compiler::Node* lhs, compiler::Node* rhs,
    compiler::Node* context) {
  typedef compiler::Node Node;

  Label return_true(this), return_false(this), end(this);
  Variable result(this, MachineRepresentation::kTagged);

  // Shared entry for floating point comparison.
  Label do_fcmp(this);
  Variable var_fcmp_lhs(this, MachineRepresentation::kFloat64),
      var_fcmp_rhs(this, MachineRepresentation::kFloat64);

  // We might need to loop several times due to ToPrimitive and/or ToNumber
  // conversions.
  Variable var_lhs(this, MachineRepresentation::kTagged),
      var_rhs(this, MachineRepresentation::kTagged);
  Variable* loop_vars[2] = {&var_lhs, &var_rhs};
  Label loop(this, 2, loop_vars);
  var_lhs.Bind(lhs);
  var_rhs.Bind(rhs);
  Goto(&loop);
  Bind(&loop);
  {
    // Load the current {lhs} and {rhs} values.
    lhs = var_lhs.value();
    rhs = var_rhs.value();

    // Check if the {lhs} is a Smi or a HeapObject.
    Label if_lhsissmi(this), if_lhsisnotsmi(this);
    Branch(TaggedIsSmi(lhs), &if_lhsissmi, &if_lhsisnotsmi);

    Bind(&if_lhsissmi);
    {
      // Check if {rhs} is a Smi or a HeapObject.
      Label if_rhsissmi(this), if_rhsisnotsmi(this);
      Branch(TaggedIsSmi(rhs), &if_rhsissmi, &if_rhsisnotsmi);

      Bind(&if_rhsissmi);
      {
        // Both {lhs} and {rhs} are Smi, so just perform a fast Smi comparison.
        switch (mode) {
          case kLessThan:
            BranchIfSmiLessThan(lhs, rhs, &return_true, &return_false);
            break;
          case kLessThanOrEqual:
            BranchIfSmiLessThanOrEqual(lhs, rhs, &return_true, &return_false);
            break;
          case kGreaterThan:
            BranchIfSmiLessThan(rhs, lhs, &return_true, &return_false);
            break;
          case kGreaterThanOrEqual:
            BranchIfSmiLessThanOrEqual(rhs, lhs, &return_true, &return_false);
            break;
        }
      }

      Bind(&if_rhsisnotsmi);
      {
        // Load the map of {rhs}.
        Node* rhs_map = LoadMap(rhs);

        // Check if the {rhs} is a HeapNumber.
        Label if_rhsisnumber(this), if_rhsisnotnumber(this, Label::kDeferred);
        Branch(IsHeapNumberMap(rhs_map), &if_rhsisnumber, &if_rhsisnotnumber);

        Bind(&if_rhsisnumber);
        {
          // Convert the {lhs} and {rhs} to floating point values, and
          // perform a floating point comparison.
          var_fcmp_lhs.Bind(SmiToFloat64(lhs));
          var_fcmp_rhs.Bind(LoadHeapNumberValue(rhs));
          Goto(&do_fcmp);
        }

        Bind(&if_rhsisnotnumber);
        {
          // Convert the {rhs} to a Number; we don't need to perform the
          // dedicated ToPrimitive(rhs, hint Number) operation, as the
          // ToNumber(rhs) will by itself already invoke ToPrimitive with
          // a Number hint.
          Callable callable = CodeFactory::NonNumberToNumber(isolate());
          var_rhs.Bind(CallStub(callable, context, rhs));
          Goto(&loop);
        }
      }
    }

    Bind(&if_lhsisnotsmi);
    {
      // Load the HeapNumber map for later comparisons.
      Node* number_map = HeapNumberMapConstant();

      // Load the map of {lhs}.
      Node* lhs_map = LoadMap(lhs);

      // Check if {rhs} is a Smi or a HeapObject.
      Label if_rhsissmi(this), if_rhsisnotsmi(this);
      Branch(TaggedIsSmi(rhs), &if_rhsissmi, &if_rhsisnotsmi);

      Bind(&if_rhsissmi);
      {
        // Check if the {lhs} is a HeapNumber.
        Label if_lhsisnumber(this), if_lhsisnotnumber(this, Label::kDeferred);
        Branch(WordEqual(lhs_map, number_map), &if_lhsisnumber,
               &if_lhsisnotnumber);

        Bind(&if_lhsisnumber);
        {
          // Convert the {lhs} and {rhs} to floating point values, and
          // perform a floating point comparison.
          var_fcmp_lhs.Bind(LoadHeapNumberValue(lhs));
          var_fcmp_rhs.Bind(SmiToFloat64(rhs));
          Goto(&do_fcmp);
        }

        Bind(&if_lhsisnotnumber);
        {
          // Convert the {lhs} to a Number; we don't need to perform the
          // dedicated ToPrimitive(lhs, hint Number) operation, as the
          // ToNumber(lhs) will by itself already invoke ToPrimitive with
          // a Number hint.
          Callable callable = CodeFactory::NonNumberToNumber(isolate());
          var_lhs.Bind(CallStub(callable, context, lhs));
          Goto(&loop);
        }
      }

      Bind(&if_rhsisnotsmi);
      {
        // Load the map of {rhs}.
        Node* rhs_map = LoadMap(rhs);

        // Check if {lhs} is a HeapNumber.
        Label if_lhsisnumber(this), if_lhsisnotnumber(this);
        Branch(WordEqual(lhs_map, number_map), &if_lhsisnumber,
               &if_lhsisnotnumber);

        Bind(&if_lhsisnumber);
        {
          // Check if {rhs} is also a HeapNumber.
          Label if_rhsisnumber(this), if_rhsisnotnumber(this, Label::kDeferred);
          Branch(WordEqual(lhs_map, rhs_map), &if_rhsisnumber,
                 &if_rhsisnotnumber);

          Bind(&if_rhsisnumber);
          {
            // Convert the {lhs} and {rhs} to floating point values, and
            // perform a floating point comparison.
            var_fcmp_lhs.Bind(LoadHeapNumberValue(lhs));
            var_fcmp_rhs.Bind(LoadHeapNumberValue(rhs));
            Goto(&do_fcmp);
          }

          Bind(&if_rhsisnotnumber);
          {
            // Convert the {rhs} to a Number; we don't need to perform
            // dedicated ToPrimitive(rhs, hint Number) operation, as the
            // ToNumber(rhs) will by itself already invoke ToPrimitive with
            // a Number hint.
            Callable callable = CodeFactory::NonNumberToNumber(isolate());
            var_rhs.Bind(CallStub(callable, context, rhs));
            Goto(&loop);
          }
        }

        Bind(&if_lhsisnotnumber);
        {
          // Load the instance type of {lhs}.
          Node* lhs_instance_type = LoadMapInstanceType(lhs_map);

          // Check if {lhs} is a String.
          Label if_lhsisstring(this), if_lhsisnotstring(this, Label::kDeferred);
          Branch(IsStringInstanceType(lhs_instance_type), &if_lhsisstring,
                 &if_lhsisnotstring);

          Bind(&if_lhsisstring);
          {
            // Load the instance type of {rhs}.
            Node* rhs_instance_type = LoadMapInstanceType(rhs_map);

            // Check if {rhs} is also a String.
            Label if_rhsisstring(this, Label::kDeferred),
                if_rhsisnotstring(this, Label::kDeferred);
            Branch(IsStringInstanceType(rhs_instance_type), &if_rhsisstring,
                   &if_rhsisnotstring);

            Bind(&if_rhsisstring);
            {
              // Both {lhs} and {rhs} are strings.
              switch (mode) {
                case kLessThan:
                  result.Bind(CallStub(CodeFactory::StringLessThan(isolate()),
                                       context, lhs, rhs));
                  Goto(&end);
                  break;
                case kLessThanOrEqual:
                  result.Bind(
                      CallStub(CodeFactory::StringLessThanOrEqual(isolate()),
                               context, lhs, rhs));
                  Goto(&end);
                  break;
                case kGreaterThan:
                  result.Bind(
                      CallStub(CodeFactory::StringGreaterThan(isolate()),
                               context, lhs, rhs));
                  Goto(&end);
                  break;
                case kGreaterThanOrEqual:
                  result.Bind(
                      CallStub(CodeFactory::StringGreaterThanOrEqual(isolate()),
                               context, lhs, rhs));
                  Goto(&end);
                  break;
              }
            }

            Bind(&if_rhsisnotstring);
            {
              // The {lhs} is a String, while {rhs} is neither a Number nor a
              // String, so we need to call ToPrimitive(rhs, hint Number) if
              // {rhs} is a receiver or ToNumber(lhs) and ToNumber(rhs) in the
              // other cases.
              STATIC_ASSERT(LAST_JS_RECEIVER_TYPE == LAST_TYPE);
              Label if_rhsisreceiver(this, Label::kDeferred),
                  if_rhsisnotreceiver(this, Label::kDeferred);
              Branch(IsJSReceiverInstanceType(rhs_instance_type),
                     &if_rhsisreceiver, &if_rhsisnotreceiver);

              Bind(&if_rhsisreceiver);
              {
                // Convert {rhs} to a primitive first passing Number hint.
                Callable callable = CodeFactory::NonPrimitiveToPrimitive(
                    isolate(), ToPrimitiveHint::kNumber);
                var_rhs.Bind(CallStub(callable, context, rhs));
                Goto(&loop);
              }

              Bind(&if_rhsisnotreceiver);
              {
                // Convert both {lhs} and {rhs} to Number.
                Callable callable = CodeFactory::ToNumber(isolate());
                var_lhs.Bind(CallStub(callable, context, lhs));
                var_rhs.Bind(CallStub(callable, context, rhs));
                Goto(&loop);
              }
            }
          }

          Bind(&if_lhsisnotstring);
          {
            // The {lhs} is neither a Number nor a String, so we need to call
            // ToPrimitive(lhs, hint Number) if {lhs} is a receiver or
            // ToNumber(lhs) and ToNumber(rhs) in the other cases.
            STATIC_ASSERT(LAST_JS_RECEIVER_TYPE == LAST_TYPE);
            Label if_lhsisreceiver(this, Label::kDeferred),
                if_lhsisnotreceiver(this, Label::kDeferred);
            Branch(IsJSReceiverInstanceType(lhs_instance_type),
                   &if_lhsisreceiver, &if_lhsisnotreceiver);

            Bind(&if_lhsisreceiver);
            {
              // Convert {lhs} to a primitive first passing Number hint.
              Callable callable = CodeFactory::NonPrimitiveToPrimitive(
                  isolate(), ToPrimitiveHint::kNumber);
              var_lhs.Bind(CallStub(callable, context, lhs));
              Goto(&loop);
            }

            Bind(&if_lhsisnotreceiver);
            {
              // Convert both {lhs} and {rhs} to Number.
              Callable callable = CodeFactory::ToNumber(isolate());
              var_lhs.Bind(CallStub(callable, context, lhs));
              var_rhs.Bind(CallStub(callable, context, rhs));
              Goto(&loop);
            }
          }
        }
      }
    }
  }

  Bind(&do_fcmp);
  {
    // Load the {lhs} and {rhs} floating point values.
    Node* lhs = var_fcmp_lhs.value();
    Node* rhs = var_fcmp_rhs.value();

    // Perform a fast floating point comparison.
    switch (mode) {
      case kLessThan:
        Branch(Float64LessThan(lhs, rhs), &return_true, &return_false);
        break;
      case kLessThanOrEqual:
        Branch(Float64LessThanOrEqual(lhs, rhs), &return_true, &return_false);
        break;
      case kGreaterThan:
        Branch(Float64GreaterThan(lhs, rhs), &return_true, &return_false);
        break;
      case kGreaterThanOrEqual:
        Branch(Float64GreaterThanOrEqual(lhs, rhs), &return_true,
               &return_false);
        break;
    }
  }

  Bind(&return_true);
  {
    result.Bind(BooleanConstant(true));
    Goto(&end);
  }

  Bind(&return_false);
  {
    result.Bind(BooleanConstant(false));
    Goto(&end);
  }

  Bind(&end);
  return result.value();
}

namespace {

void GenerateEqual_Same(CodeStubAssembler* assembler, compiler::Node* value,
                        CodeStubAssembler::Label* if_equal,
                        CodeStubAssembler::Label* if_notequal) {
  // In case of abstract or strict equality checks, we need additional checks
  // for NaN values because they are not considered equal, even if both the
  // left and the right hand side reference exactly the same value.
  // TODO(bmeurer): This seems to violate the SIMD.js specification, but it
  // seems to be what is tested in the current SIMD.js testsuite.

  typedef CodeStubAssembler::Label Label;
  typedef compiler::Node Node;

  // Check if {value} is a Smi or a HeapObject.
  Label if_valueissmi(assembler), if_valueisnotsmi(assembler);
  assembler->Branch(assembler->TaggedIsSmi(value), &if_valueissmi,
                    &if_valueisnotsmi);

  assembler->Bind(&if_valueisnotsmi);
  {
    // Load the map of {value}.
    Node* value_map = assembler->LoadMap(value);

    // Check if {value} (and therefore {rhs}) is a HeapNumber.
    Label if_valueisnumber(assembler), if_valueisnotnumber(assembler);
    assembler->Branch(assembler->IsHeapNumberMap(value_map), &if_valueisnumber,
                      &if_valueisnotnumber);

    assembler->Bind(&if_valueisnumber);
    {
      // Convert {value} (and therefore {rhs}) to floating point value.
      Node* value_value = assembler->LoadHeapNumberValue(value);

      // Check if the HeapNumber value is a NaN.
      assembler->BranchIfFloat64IsNaN(value_value, if_notequal, if_equal);
    }

    assembler->Bind(&if_valueisnotnumber);
    assembler->Goto(if_equal);
  }

  assembler->Bind(&if_valueissmi);
  assembler->Goto(if_equal);
}

void GenerateEqual_Simd128Value_HeapObject(
    CodeStubAssembler* assembler, compiler::Node* lhs, compiler::Node* lhs_map,
    compiler::Node* rhs, compiler::Node* rhs_map,
    CodeStubAssembler::Label* if_equal, CodeStubAssembler::Label* if_notequal) {
  assembler->BranchIfSimd128Equal(lhs, lhs_map, rhs, rhs_map, if_equal,
                                  if_notequal);
}

}  // namespace

// ES6 section 7.2.12 Abstract Equality Comparison
compiler::Node* CodeStubAssembler::Equal(ResultMode mode, compiler::Node* lhs,
                                         compiler::Node* rhs,
                                         compiler::Node* context) {
  // This is a slightly optimized version of Object::Equals represented as
  // scheduled TurboFan graph utilizing the CodeStubAssembler. Whenever you
  // change something functionality wise in here, remember to update the
  // Object::Equals method as well.
  typedef compiler::Node Node;

  Label if_equal(this), if_notequal(this),
      do_rhsstringtonumber(this, Label::kDeferred), end(this);
  Variable result(this, MachineRepresentation::kTagged);

  // Shared entry for floating point comparison.
  Label do_fcmp(this);
  Variable var_fcmp_lhs(this, MachineRepresentation::kFloat64),
      var_fcmp_rhs(this, MachineRepresentation::kFloat64);

  // We might need to loop several times due to ToPrimitive and/or ToNumber
  // conversions.
  Variable var_lhs(this, MachineRepresentation::kTagged),
      var_rhs(this, MachineRepresentation::kTagged);
  Variable* loop_vars[2] = {&var_lhs, &var_rhs};
  Label loop(this, 2, loop_vars);
  var_lhs.Bind(lhs);
  var_rhs.Bind(rhs);
  Goto(&loop);
  Bind(&loop);
  {
    // Load the current {lhs} and {rhs} values.
    lhs = var_lhs.value();
    rhs = var_rhs.value();

    // Check if {lhs} and {rhs} refer to the same object.
    Label if_same(this), if_notsame(this);
    Branch(WordEqual(lhs, rhs), &if_same, &if_notsame);

    Bind(&if_same);
    {
      // The {lhs} and {rhs} reference the exact same value, yet we need special
      // treatment for HeapNumber, as NaN is not equal to NaN.
      GenerateEqual_Same(this, lhs, &if_equal, &if_notequal);
    }

    Bind(&if_notsame);
    {
      // Check if {lhs} is a Smi or a HeapObject.
      Label if_lhsissmi(this), if_lhsisnotsmi(this);
      Branch(TaggedIsSmi(lhs), &if_lhsissmi, &if_lhsisnotsmi);

      Bind(&if_lhsissmi);
      {
        // Check if {rhs} is a Smi or a HeapObject.
        Label if_rhsissmi(this), if_rhsisnotsmi(this);
        Branch(TaggedIsSmi(rhs), &if_rhsissmi, &if_rhsisnotsmi);

        Bind(&if_rhsissmi);
        // We have already checked for {lhs} and {rhs} being the same value, so
        // if both are Smis when we get here they must not be equal.
        Goto(&if_notequal);

        Bind(&if_rhsisnotsmi);
        {
          // Load the map of {rhs}.
          Node* rhs_map = LoadMap(rhs);

          // Check if {rhs} is a HeapNumber.
          Node* number_map = HeapNumberMapConstant();
          Label if_rhsisnumber(this), if_rhsisnotnumber(this);
          Branch(WordEqual(rhs_map, number_map), &if_rhsisnumber,
                 &if_rhsisnotnumber);

          Bind(&if_rhsisnumber);
          {
            // Convert {lhs} and {rhs} to floating point values, and
            // perform a floating point comparison.
            var_fcmp_lhs.Bind(SmiToFloat64(lhs));
            var_fcmp_rhs.Bind(LoadHeapNumberValue(rhs));
            Goto(&do_fcmp);
          }

          Bind(&if_rhsisnotnumber);
          {
            // Load the instance type of the {rhs}.
            Node* rhs_instance_type = LoadMapInstanceType(rhs_map);

            // Check if the {rhs} is a String.
            Label if_rhsisstring(this, Label::kDeferred),
                if_rhsisnotstring(this);
            Branch(IsStringInstanceType(rhs_instance_type), &if_rhsisstring,
                   &if_rhsisnotstring);

            Bind(&if_rhsisstring);
            {
              // The {rhs} is a String and the {lhs} is a Smi; we need
              // to convert the {rhs} to a Number and compare the output to
              // the Number on the {lhs}.
              Goto(&do_rhsstringtonumber);
            }

            Bind(&if_rhsisnotstring);
            {
              // Check if the {rhs} is a Boolean.
              Label if_rhsisboolean(this), if_rhsisnotboolean(this);
              Branch(IsBooleanMap(rhs_map), &if_rhsisboolean,
                     &if_rhsisnotboolean);

              Bind(&if_rhsisboolean);
              {
                // The {rhs} is a Boolean, load its number value.
                var_rhs.Bind(LoadObjectField(rhs, Oddball::kToNumberOffset));
                Goto(&loop);
              }

              Bind(&if_rhsisnotboolean);
              {
                // Check if the {rhs} is a Receiver.
                STATIC_ASSERT(LAST_JS_RECEIVER_TYPE == LAST_TYPE);
                Label if_rhsisreceiver(this, Label::kDeferred),
                    if_rhsisnotreceiver(this);
                Branch(IsJSReceiverInstanceType(rhs_instance_type),
                       &if_rhsisreceiver, &if_rhsisnotreceiver);

                Bind(&if_rhsisreceiver);
                {
                  // Convert {rhs} to a primitive first (passing no hint).
                  Callable callable =
                      CodeFactory::NonPrimitiveToPrimitive(isolate());
                  var_rhs.Bind(CallStub(callable, context, rhs));
                  Goto(&loop);
                }

                Bind(&if_rhsisnotreceiver);
                Goto(&if_notequal);
              }
            }
          }
        }
      }

      Bind(&if_lhsisnotsmi);
      {
        // Check if {rhs} is a Smi or a HeapObject.
        Label if_rhsissmi(this), if_rhsisnotsmi(this);
        Branch(TaggedIsSmi(rhs), &if_rhsissmi, &if_rhsisnotsmi);

        Bind(&if_rhsissmi);
        {
          // The {lhs} is a HeapObject and the {rhs} is a Smi; swapping {lhs}
          // and {rhs} is not observable and doesn't matter for the result, so
          // we can just swap them and use the Smi handling above (for {lhs}
          // being a Smi).
          var_lhs.Bind(rhs);
          var_rhs.Bind(lhs);
          Goto(&loop);
        }

        Bind(&if_rhsisnotsmi);
        {
          Label if_lhsisstring(this), if_lhsisnumber(this),
              if_lhsissymbol(this), if_lhsissimd128value(this),
              if_lhsisoddball(this), if_lhsisreceiver(this);

          // Both {lhs} and {rhs} are HeapObjects, load their maps
          // and their instance types.
          Node* lhs_map = LoadMap(lhs);
          Node* rhs_map = LoadMap(rhs);

          // Load the instance types of {lhs} and {rhs}.
          Node* lhs_instance_type = LoadMapInstanceType(lhs_map);
          Node* rhs_instance_type = LoadMapInstanceType(rhs_map);

          // Dispatch based on the instance type of {lhs}.
          size_t const kNumCases = FIRST_NONSTRING_TYPE + 4;
          Label* case_labels[kNumCases];
          int32_t case_values[kNumCases];
          for (int32_t i = 0; i < FIRST_NONSTRING_TYPE; ++i) {
            case_labels[i] = new Label(this);
            case_values[i] = i;
          }
          case_labels[FIRST_NONSTRING_TYPE + 0] = &if_lhsisnumber;
          case_values[FIRST_NONSTRING_TYPE + 0] = HEAP_NUMBER_TYPE;
          case_labels[FIRST_NONSTRING_TYPE + 1] = &if_lhsissymbol;
          case_values[FIRST_NONSTRING_TYPE + 1] = SYMBOL_TYPE;
          case_labels[FIRST_NONSTRING_TYPE + 2] = &if_lhsissimd128value;
          case_values[FIRST_NONSTRING_TYPE + 2] = SIMD128_VALUE_TYPE;
          case_labels[FIRST_NONSTRING_TYPE + 3] = &if_lhsisoddball;
          case_values[FIRST_NONSTRING_TYPE + 3] = ODDBALL_TYPE;
          Switch(lhs_instance_type, &if_lhsisreceiver, case_values, case_labels,
                 arraysize(case_values));
          for (int32_t i = 0; i < FIRST_NONSTRING_TYPE; ++i) {
            Bind(case_labels[i]);
            Goto(&if_lhsisstring);
            delete case_labels[i];
          }

          Bind(&if_lhsisstring);
          {
            // Check if {rhs} is also a String.
            Label if_rhsisstring(this, Label::kDeferred),
                if_rhsisnotstring(this);
            Branch(IsStringInstanceType(rhs_instance_type), &if_rhsisstring,
                   &if_rhsisnotstring);

            Bind(&if_rhsisstring);
            {
              // Both {lhs} and {rhs} are of type String, just do the
              // string comparison then.
              Callable callable = (mode == kDontNegateResult)
                                      ? CodeFactory::StringEqual(isolate())
                                      : CodeFactory::StringNotEqual(isolate());
              result.Bind(CallStub(callable, context, lhs, rhs));
              Goto(&end);
            }

            Bind(&if_rhsisnotstring);
            {
              // The {lhs} is a String and the {rhs} is some other HeapObject.
              // Swapping {lhs} and {rhs} is not observable and doesn't matter
              // for the result, so we can just swap them and use the String
              // handling below (for {rhs} being a String).
              var_lhs.Bind(rhs);
              var_rhs.Bind(lhs);
              Goto(&loop);
            }
          }

          Bind(&if_lhsisnumber);
          {
            // Check if {rhs} is also a HeapNumber.
            Label if_rhsisnumber(this), if_rhsisnotnumber(this);
            Branch(Word32Equal(lhs_instance_type, rhs_instance_type),
                   &if_rhsisnumber, &if_rhsisnotnumber);

            Bind(&if_rhsisnumber);
            {
              // Convert {lhs} and {rhs} to floating point values, and
              // perform a floating point comparison.
              var_fcmp_lhs.Bind(LoadHeapNumberValue(lhs));
              var_fcmp_rhs.Bind(LoadHeapNumberValue(rhs));
              Goto(&do_fcmp);
            }

            Bind(&if_rhsisnotnumber);
            {
              // The {lhs} is a Number, the {rhs} is some other HeapObject.
              Label if_rhsisstring(this, Label::kDeferred),
                  if_rhsisnotstring(this);
              Branch(IsStringInstanceType(rhs_instance_type), &if_rhsisstring,
                     &if_rhsisnotstring);

              Bind(&if_rhsisstring);
              {
                // The {rhs} is a String and the {lhs} is a HeapNumber; we need
                // to convert the {rhs} to a Number and compare the output to
                // the Number on the {lhs}.
                Goto(&do_rhsstringtonumber);
              }

              Bind(&if_rhsisnotstring);
              {
                // Check if the {rhs} is a JSReceiver.
                Label if_rhsisreceiver(this), if_rhsisnotreceiver(this);
                STATIC_ASSERT(LAST_TYPE == LAST_JS_RECEIVER_TYPE);
                Branch(IsJSReceiverInstanceType(rhs_instance_type),
                       &if_rhsisreceiver, &if_rhsisnotreceiver);

                Bind(&if_rhsisreceiver);
                {
                  // The {lhs} is a Primitive and the {rhs} is a JSReceiver.
                  // Swapping {lhs} and {rhs} is not observable and doesn't
                  // matter for the result, so we can just swap them and use
                  // the JSReceiver handling below (for {lhs} being a
                  // JSReceiver).
                  var_lhs.Bind(rhs);
                  var_rhs.Bind(lhs);
                  Goto(&loop);
                }

                Bind(&if_rhsisnotreceiver);
                {
                  // Check if {rhs} is a Boolean.
                  Label if_rhsisboolean(this), if_rhsisnotboolean(this);
                  Branch(IsBooleanMap(rhs_map), &if_rhsisboolean,
                         &if_rhsisnotboolean);

                  Bind(&if_rhsisboolean);
                  {
                    // The {rhs} is a Boolean, convert it to a Smi first.
                    var_rhs.Bind(
                        LoadObjectField(rhs, Oddball::kToNumberOffset));
                    Goto(&loop);
                  }

                  Bind(&if_rhsisnotboolean);
                  Goto(&if_notequal);
                }
              }
            }
          }

          Bind(&if_lhsisoddball);
          {
            // The {lhs} is an Oddball and {rhs} is some other HeapObject.
            Label if_lhsisboolean(this), if_lhsisnotboolean(this);
            Node* boolean_map = BooleanMapConstant();
            Branch(WordEqual(lhs_map, boolean_map), &if_lhsisboolean,
                   &if_lhsisnotboolean);

            Bind(&if_lhsisboolean);
            {
              // The {lhs} is a Boolean, check if {rhs} is also a Boolean.
              Label if_rhsisboolean(this), if_rhsisnotboolean(this);
              Branch(WordEqual(rhs_map, boolean_map), &if_rhsisboolean,
                     &if_rhsisnotboolean);

              Bind(&if_rhsisboolean);
              {
                // Both {lhs} and {rhs} are distinct Boolean values.
                Goto(&if_notequal);
              }

              Bind(&if_rhsisnotboolean);
              {
                // Convert the {lhs} to a Number first.
                var_lhs.Bind(LoadObjectField(lhs, Oddball::kToNumberOffset));
                Goto(&loop);
              }
            }

            Bind(&if_lhsisnotboolean);
            {
              // The {lhs} is either Null or Undefined; check if the {rhs} is
              // undetectable (i.e. either also Null or Undefined or some
              // undetectable JSReceiver).
              Node* rhs_bitfield = LoadMapBitField(rhs_map);
              Branch(Word32Equal(
                         Word32And(rhs_bitfield,
                                   Int32Constant(1 << Map::kIsUndetectable)),
                         Int32Constant(0)),
                     &if_notequal, &if_equal);
            }
          }

          Bind(&if_lhsissymbol);
          {
            // Check if the {rhs} is a JSReceiver.
            Label if_rhsisreceiver(this), if_rhsisnotreceiver(this);
            STATIC_ASSERT(LAST_TYPE == LAST_JS_RECEIVER_TYPE);
            Branch(IsJSReceiverInstanceType(rhs_instance_type),
                   &if_rhsisreceiver, &if_rhsisnotreceiver);

            Bind(&if_rhsisreceiver);
            {
              // The {lhs} is a Primitive and the {rhs} is a JSReceiver.
              // Swapping {lhs} and {rhs} is not observable and doesn't
              // matter for the result, so we can just swap them and use
              // the JSReceiver handling below (for {lhs} being a JSReceiver).
              var_lhs.Bind(rhs);
              var_rhs.Bind(lhs);
              Goto(&loop);
            }

            Bind(&if_rhsisnotreceiver);
            {
              // The {rhs} is not a JSReceiver and also not the same Symbol
              // as the {lhs}, so this is equality check is considered false.
              Goto(&if_notequal);
            }
          }

          Bind(&if_lhsissimd128value);
          {
            // Check if the {rhs} is also a Simd128Value.
            Label if_rhsissimd128value(this), if_rhsisnotsimd128value(this);
            Branch(Word32Equal(lhs_instance_type, rhs_instance_type),
                   &if_rhsissimd128value, &if_rhsisnotsimd128value);

            Bind(&if_rhsissimd128value);
            {
              // Both {lhs} and {rhs} is a Simd128Value.
              GenerateEqual_Simd128Value_HeapObject(
                  this, lhs, lhs_map, rhs, rhs_map, &if_equal, &if_notequal);
            }

            Bind(&if_rhsisnotsimd128value);
            {
              // Check if the {rhs} is a JSReceiver.
              Label if_rhsisreceiver(this), if_rhsisnotreceiver(this);
              STATIC_ASSERT(LAST_TYPE == LAST_JS_RECEIVER_TYPE);
              Branch(IsJSReceiverInstanceType(rhs_instance_type),
                     &if_rhsisreceiver, &if_rhsisnotreceiver);

              Bind(&if_rhsisreceiver);
              {
                // The {lhs} is a Primitive and the {rhs} is a JSReceiver.
                // Swapping {lhs} and {rhs} is not observable and doesn't
                // matter for the result, so we can just swap them and use
                // the JSReceiver handling below (for {lhs} being a JSReceiver).
                var_lhs.Bind(rhs);
                var_rhs.Bind(lhs);
                Goto(&loop);
              }

              Bind(&if_rhsisnotreceiver);
              {
                // The {rhs} is some other Primitive.
                Goto(&if_notequal);
              }
            }
          }

          Bind(&if_lhsisreceiver);
          {
            // Check if the {rhs} is also a JSReceiver.
            Label if_rhsisreceiver(this), if_rhsisnotreceiver(this);
            STATIC_ASSERT(LAST_TYPE == LAST_JS_RECEIVER_TYPE);
            Branch(IsJSReceiverInstanceType(rhs_instance_type),
                   &if_rhsisreceiver, &if_rhsisnotreceiver);

            Bind(&if_rhsisreceiver);
            {
              // Both {lhs} and {rhs} are different JSReceiver references, so
              // this cannot be considered equal.
              Goto(&if_notequal);
            }

            Bind(&if_rhsisnotreceiver);
            {
              // Check if {rhs} is Null or Undefined (an undetectable check
              // is sufficient here, since we already know that {rhs} is not
              // a JSReceiver).
              Label if_rhsisundetectable(this),
                  if_rhsisnotundetectable(this, Label::kDeferred);
              Node* rhs_bitfield = LoadMapBitField(rhs_map);
              Branch(Word32Equal(
                         Word32And(rhs_bitfield,
                                   Int32Constant(1 << Map::kIsUndetectable)),
                         Int32Constant(0)),
                     &if_rhsisnotundetectable, &if_rhsisundetectable);

              Bind(&if_rhsisundetectable);
              {
                // Check if {lhs} is an undetectable JSReceiver.
                Node* lhs_bitfield = LoadMapBitField(lhs_map);
                Branch(Word32Equal(
                           Word32And(lhs_bitfield,
                                     Int32Constant(1 << Map::kIsUndetectable)),
                           Int32Constant(0)),
                       &if_notequal, &if_equal);
              }

              Bind(&if_rhsisnotundetectable);
              {
                // The {rhs} is some Primitive different from Null and
                // Undefined, need to convert {lhs} to Primitive first.
                Callable callable =
                    CodeFactory::NonPrimitiveToPrimitive(isolate());
                var_lhs.Bind(CallStub(callable, context, lhs));
                Goto(&loop);
              }
            }
          }
        }
      }
    }

    Bind(&do_rhsstringtonumber);
    {
      Callable callable = CodeFactory::StringToNumber(isolate());
      var_rhs.Bind(CallStub(callable, context, rhs));
      Goto(&loop);
    }
  }

  Bind(&do_fcmp);
  {
    // Load the {lhs} and {rhs} floating point values.
    Node* lhs = var_fcmp_lhs.value();
    Node* rhs = var_fcmp_rhs.value();

    // Perform a fast floating point comparison.
    Branch(Float64Equal(lhs, rhs), &if_equal, &if_notequal);
  }

  Bind(&if_equal);
  {
    result.Bind(BooleanConstant(mode == kDontNegateResult));
    Goto(&end);
  }

  Bind(&if_notequal);
  {
    result.Bind(BooleanConstant(mode == kNegateResult));
    Goto(&end);
  }

  Bind(&end);
  return result.value();
}

compiler::Node* CodeStubAssembler::StrictEqual(ResultMode mode,
                                               compiler::Node* lhs,
                                               compiler::Node* rhs,
                                               compiler::Node* context) {
  // Here's pseudo-code for the algorithm below in case of kDontNegateResult
  // mode; for kNegateResult mode we properly negate the result.
  //
  // if (lhs == rhs) {
  //   if (lhs->IsHeapNumber()) return HeapNumber::cast(lhs)->value() != NaN;
  //   return true;
  // }
  // if (!lhs->IsSmi()) {
  //   if (lhs->IsHeapNumber()) {
  //     if (rhs->IsSmi()) {
  //       return Smi::cast(rhs)->value() == HeapNumber::cast(lhs)->value();
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
  //       } else if (lhs->IsSimd128()) {
  //         if (rhs->IsSimd128()) {
  //           return %StrictEqual(lhs, rhs);
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
  //       return Smi::cast(lhs)->value() == HeapNumber::cast(rhs)->value();
  //     } else {
  //       return false;
  //     }
  //   }
  // }

  typedef compiler::Node Node;

  Label if_equal(this), if_notequal(this), end(this);
  Variable result(this, MachineRepresentation::kTagged);

  // Check if {lhs} and {rhs} refer to the same object.
  Label if_same(this), if_notsame(this);
  Branch(WordEqual(lhs, rhs), &if_same, &if_notsame);

  Bind(&if_same);
  {
    // The {lhs} and {rhs} reference the exact same value, yet we need special
    // treatment for HeapNumber, as NaN is not equal to NaN.
    GenerateEqual_Same(this, lhs, &if_equal, &if_notequal);
  }

  Bind(&if_notsame);
  {
    // The {lhs} and {rhs} reference different objects, yet for Smi, HeapNumber,
    // String and Simd128Value they can still be considered equal.
    Node* number_map = HeapNumberMapConstant();

    // Check if {lhs} is a Smi or a HeapObject.
    Label if_lhsissmi(this), if_lhsisnotsmi(this);
    Branch(TaggedIsSmi(lhs), &if_lhsissmi, &if_lhsisnotsmi);

    Bind(&if_lhsisnotsmi);
    {
      // Load the map of {lhs}.
      Node* lhs_map = LoadMap(lhs);

      // Check if {lhs} is a HeapNumber.
      Label if_lhsisnumber(this), if_lhsisnotnumber(this);
      Branch(WordEqual(lhs_map, number_map), &if_lhsisnumber,
             &if_lhsisnotnumber);

      Bind(&if_lhsisnumber);
      {
        // Check if {rhs} is a Smi or a HeapObject.
        Label if_rhsissmi(this), if_rhsisnotsmi(this);
        Branch(TaggedIsSmi(rhs), &if_rhsissmi, &if_rhsisnotsmi);

        Bind(&if_rhsissmi);
        {
          // Convert {lhs} and {rhs} to floating point values.
          Node* lhs_value = LoadHeapNumberValue(lhs);
          Node* rhs_value = SmiToFloat64(rhs);

          // Perform a floating point comparison of {lhs} and {rhs}.
          Branch(Float64Equal(lhs_value, rhs_value), &if_equal, &if_notequal);
        }

        Bind(&if_rhsisnotsmi);
        {
          // Load the map of {rhs}.
          Node* rhs_map = LoadMap(rhs);

          // Check if {rhs} is also a HeapNumber.
          Label if_rhsisnumber(this), if_rhsisnotnumber(this);
          Branch(WordEqual(rhs_map, number_map), &if_rhsisnumber,
                 &if_rhsisnotnumber);

          Bind(&if_rhsisnumber);
          {
            // Convert {lhs} and {rhs} to floating point values.
            Node* lhs_value = LoadHeapNumberValue(lhs);
            Node* rhs_value = LoadHeapNumberValue(rhs);

            // Perform a floating point comparison of {lhs} and {rhs}.
            Branch(Float64Equal(lhs_value, rhs_value), &if_equal, &if_notequal);
          }

          Bind(&if_rhsisnotnumber);
          Goto(&if_notequal);
        }
      }

      Bind(&if_lhsisnotnumber);
      {
        // Check if {rhs} is a Smi or a HeapObject.
        Label if_rhsissmi(this), if_rhsisnotsmi(this);
        Branch(TaggedIsSmi(rhs), &if_rhsissmi, &if_rhsisnotsmi);

        Bind(&if_rhsissmi);
        Goto(&if_notequal);

        Bind(&if_rhsisnotsmi);
        {
          // Load the instance type of {lhs}.
          Node* lhs_instance_type = LoadMapInstanceType(lhs_map);

          // Check if {lhs} is a String.
          Label if_lhsisstring(this), if_lhsisnotstring(this);
          Branch(IsStringInstanceType(lhs_instance_type), &if_lhsisstring,
                 &if_lhsisnotstring);

          Bind(&if_lhsisstring);
          {
            // Load the instance type of {rhs}.
            Node* rhs_instance_type = LoadInstanceType(rhs);

            // Check if {rhs} is also a String.
            Label if_rhsisstring(this, Label::kDeferred),
                if_rhsisnotstring(this);
            Branch(IsStringInstanceType(rhs_instance_type), &if_rhsisstring,
                   &if_rhsisnotstring);

            Bind(&if_rhsisstring);
            {
              Callable callable = (mode == kDontNegateResult)
                                      ? CodeFactory::StringEqual(isolate())
                                      : CodeFactory::StringNotEqual(isolate());
              result.Bind(CallStub(callable, context, lhs, rhs));
              Goto(&end);
            }

            Bind(&if_rhsisnotstring);
            Goto(&if_notequal);
          }

          Bind(&if_lhsisnotstring);
          {
            // Check if {lhs} is a Simd128Value.
            Label if_lhsissimd128value(this), if_lhsisnotsimd128value(this);
            Branch(Word32Equal(lhs_instance_type,
                               Int32Constant(SIMD128_VALUE_TYPE)),
                   &if_lhsissimd128value, &if_lhsisnotsimd128value);

            Bind(&if_lhsissimd128value);
            {
              // Load the map of {rhs}.
              Node* rhs_map = LoadMap(rhs);

              // Check if {rhs} is also a Simd128Value that is equal to {lhs}.
              GenerateEqual_Simd128Value_HeapObject(
                  this, lhs, lhs_map, rhs, rhs_map, &if_equal, &if_notequal);
            }

            Bind(&if_lhsisnotsimd128value);
            Goto(&if_notequal);
          }
        }
      }
    }

    Bind(&if_lhsissmi);
    {
      // We already know that {lhs} and {rhs} are not reference equal, and {lhs}
      // is a Smi; so {lhs} and {rhs} can only be strictly equal if {rhs} is a
      // HeapNumber with an equal floating point value.

      // Check if {rhs} is a Smi or a HeapObject.
      Label if_rhsissmi(this), if_rhsisnotsmi(this);
      Branch(TaggedIsSmi(rhs), &if_rhsissmi, &if_rhsisnotsmi);

      Bind(&if_rhsissmi);
      Goto(&if_notequal);

      Bind(&if_rhsisnotsmi);
      {
        // Load the map of the {rhs}.
        Node* rhs_map = LoadMap(rhs);

        // The {rhs} could be a HeapNumber with the same value as {lhs}.
        Label if_rhsisnumber(this), if_rhsisnotnumber(this);
        Branch(WordEqual(rhs_map, number_map), &if_rhsisnumber,
               &if_rhsisnotnumber);

        Bind(&if_rhsisnumber);
        {
          // Convert {lhs} and {rhs} to floating point values.
          Node* lhs_value = SmiToFloat64(lhs);
          Node* rhs_value = LoadHeapNumberValue(rhs);

          // Perform a floating point comparison of {lhs} and {rhs}.
          Branch(Float64Equal(lhs_value, rhs_value), &if_equal, &if_notequal);
        }

        Bind(&if_rhsisnotnumber);
        Goto(&if_notequal);
      }
    }
  }

  Bind(&if_equal);
  {
    result.Bind(BooleanConstant(mode == kDontNegateResult));
    Goto(&end);
  }

  Bind(&if_notequal);
  {
    result.Bind(BooleanConstant(mode == kNegateResult));
    Goto(&end);
  }

  Bind(&end);
  return result.value();
}

// ECMA#sec-samevalue
// This algorithm differs from the Strict Equality Comparison Algorithm in its
// treatment of signed zeroes and NaNs.
compiler::Node* CodeStubAssembler::SameValue(compiler::Node* lhs,
                                             compiler::Node* rhs,
                                             compiler::Node* context) {
  Variable var_result(this, MachineType::PointerRepresentation());
  Label strict_equal(this), out(this);

  Node* const int_false = IntPtrConstant(0);
  Node* const int_true = IntPtrConstant(1);

  Label if_equal(this), if_notequal(this);
  Branch(WordEqual(lhs, rhs), &if_equal, &if_notequal);

  Bind(&if_equal);
  {
    // This covers the case when {lhs} == {rhs}. We can simply return true
    // because SameValue considers two NaNs to be equal.

    var_result.Bind(int_true);
    Goto(&out);
  }

  Bind(&if_notequal);
  {
    // This covers the case when {lhs} != {rhs}. We only handle numbers here
    // and defer to StrictEqual for the rest.

    Node* const lhs_float = TryTaggedToFloat64(lhs, &strict_equal);
    Node* const rhs_float = TryTaggedToFloat64(rhs, &strict_equal);

    Label if_lhsisnan(this), if_lhsnotnan(this);
    BranchIfFloat64IsNaN(lhs_float, &if_lhsisnan, &if_lhsnotnan);

    Bind(&if_lhsisnan);
    {
      // Return true iff {rhs} is NaN.

      Node* const result =
          Select(Float64Equal(rhs_float, rhs_float), int_false, int_true,
                 MachineType::PointerRepresentation());
      var_result.Bind(result);
      Goto(&out);
    }

    Bind(&if_lhsnotnan);
    {
      Label if_floatisequal(this), if_floatnotequal(this);
      Branch(Float64Equal(lhs_float, rhs_float), &if_floatisequal,
             &if_floatnotequal);

      Bind(&if_floatisequal);
      {
        // We still need to handle the case when {lhs} and {rhs} are -0.0 and
        // 0.0 (or vice versa). Compare the high word to
        // distinguish between the two.

        Node* const lhs_hi_word = Float64ExtractHighWord32(lhs_float);
        Node* const rhs_hi_word = Float64ExtractHighWord32(rhs_float);

        // If x is +0 and y is -0, return false.
        // If x is -0 and y is +0, return false.

        Node* const result = Word32Equal(lhs_hi_word, rhs_hi_word);
        var_result.Bind(result);
        Goto(&out);
      }

      Bind(&if_floatnotequal);
      {
        var_result.Bind(int_false);
        Goto(&out);
      }
    }
  }

  Bind(&strict_equal);
  {
    Node* const is_equal = StrictEqual(kDontNegateResult, lhs, rhs, context);
    Node* const result = WordEqual(is_equal, TrueConstant());
    var_result.Bind(result);
    Goto(&out);
  }

  Bind(&out);
  return var_result.value();
}

compiler::Node* CodeStubAssembler::ForInFilter(compiler::Node* key,
                                               compiler::Node* object,
                                               compiler::Node* context) {
  Label return_undefined(this, Label::kDeferred), return_to_name(this),
      end(this);

  Variable var_result(this, MachineRepresentation::kTagged);

  Node* has_property =
      HasProperty(object, key, context, Runtime::kForInHasProperty);

  Branch(WordEqual(has_property, BooleanConstant(true)), &return_to_name,
         &return_undefined);

  Bind(&return_to_name);
  {
    var_result.Bind(ToName(context, key));
    Goto(&end);
  }

  Bind(&return_undefined);
  {
    var_result.Bind(UndefinedConstant());
    Goto(&end);
  }

  Bind(&end);
  return var_result.value();
}

compiler::Node* CodeStubAssembler::HasProperty(
    compiler::Node* object, compiler::Node* key, compiler::Node* context,
    Runtime::FunctionId fallback_runtime_function_id) {
  typedef compiler::Node Node;
  typedef CodeStubAssembler::Label Label;
  typedef CodeStubAssembler::Variable Variable;

  Label call_runtime(this, Label::kDeferred), return_true(this),
      return_false(this), end(this);

  CodeStubAssembler::LookupInHolder lookup_property_in_holder =
      [this, &return_true](Node* receiver, Node* holder, Node* holder_map,
                           Node* holder_instance_type, Node* unique_name,
                           Label* next_holder, Label* if_bailout) {
        TryHasOwnProperty(holder, holder_map, holder_instance_type, unique_name,
                          &return_true, next_holder, if_bailout);
      };

  CodeStubAssembler::LookupInHolder lookup_element_in_holder =
      [this, &return_true](Node* receiver, Node* holder, Node* holder_map,
                           Node* holder_instance_type, Node* index,
                           Label* next_holder, Label* if_bailout) {
        TryLookupElement(holder, holder_map, holder_instance_type, index,
                         &return_true, next_holder, if_bailout);
      };

  TryPrototypeChainLookup(object, key, lookup_property_in_holder,
                          lookup_element_in_holder, &return_false,
                          &call_runtime);

  Variable result(this, MachineRepresentation::kTagged);
  Bind(&return_true);
  {
    result.Bind(BooleanConstant(true));
    Goto(&end);
  }

  Bind(&return_false);
  {
    result.Bind(BooleanConstant(false));
    Goto(&end);
  }

  Bind(&call_runtime);
  {
    result.Bind(
        CallRuntime(fallback_runtime_function_id, context, object, key));
    Goto(&end);
  }

  Bind(&end);
  return result.value();
}

compiler::Node* CodeStubAssembler::Typeof(compiler::Node* value,
                                          compiler::Node* context) {
  Variable result_var(this, MachineRepresentation::kTagged);

  Label return_number(this, Label::kDeferred), if_oddball(this),
      return_function(this), return_undefined(this), return_object(this),
      return_string(this), return_result(this);

  GotoIf(TaggedIsSmi(value), &return_number);

  Node* map = LoadMap(value);

  GotoIf(IsHeapNumberMap(map), &return_number);

  Node* instance_type = LoadMapInstanceType(map);

  GotoIf(Word32Equal(instance_type, Int32Constant(ODDBALL_TYPE)), &if_oddball);

  Node* callable_or_undetectable_mask = Word32And(
      LoadMapBitField(map),
      Int32Constant(1 << Map::kIsCallable | 1 << Map::kIsUndetectable));

  GotoIf(Word32Equal(callable_or_undetectable_mask,
                     Int32Constant(1 << Map::kIsCallable)),
         &return_function);

  GotoUnless(Word32Equal(callable_or_undetectable_mask, Int32Constant(0)),
             &return_undefined);

  GotoIf(IsJSReceiverInstanceType(instance_type), &return_object);

  GotoIf(IsStringInstanceType(instance_type), &return_string);

#define SIMD128_BRANCH(TYPE, Type, type, lane_count, lane_type) \
  Label return_##type(this);                                    \
  Node* type##_map = HeapConstant(factory()->type##_map());     \
  GotoIf(WordEqual(map, type##_map), &return_##type);
  SIMD128_TYPES(SIMD128_BRANCH)
#undef SIMD128_BRANCH

  CSA_ASSERT(this, Word32Equal(instance_type, Int32Constant(SYMBOL_TYPE)));
  result_var.Bind(HeapConstant(isolate()->factory()->symbol_string()));
  Goto(&return_result);

  Bind(&return_number);
  {
    result_var.Bind(HeapConstant(isolate()->factory()->number_string()));
    Goto(&return_result);
  }

  Bind(&if_oddball);
  {
    Node* type = LoadObjectField(value, Oddball::kTypeOfOffset);
    result_var.Bind(type);
    Goto(&return_result);
  }

  Bind(&return_function);
  {
    result_var.Bind(HeapConstant(isolate()->factory()->function_string()));
    Goto(&return_result);
  }

  Bind(&return_undefined);
  {
    result_var.Bind(HeapConstant(isolate()->factory()->undefined_string()));
    Goto(&return_result);
  }

  Bind(&return_object);
  {
    result_var.Bind(HeapConstant(isolate()->factory()->object_string()));
    Goto(&return_result);
  }

  Bind(&return_string);
  {
    result_var.Bind(HeapConstant(isolate()->factory()->string_string()));
    Goto(&return_result);
  }

#define SIMD128_BIND_RETURN(TYPE, Type, type, lane_count, lane_type)      \
  Bind(&return_##type);                                                   \
  {                                                                       \
    result_var.Bind(HeapConstant(isolate()->factory()->type##_string())); \
    Goto(&return_result);                                                 \
  }
  SIMD128_TYPES(SIMD128_BIND_RETURN)
#undef SIMD128_BIND_RETURN

  Bind(&return_result);
  return result_var.value();
}

compiler::Node* CodeStubAssembler::InstanceOf(compiler::Node* object,
                                              compiler::Node* callable,
                                              compiler::Node* context) {
  Label return_runtime(this, Label::kDeferred), end(this);
  Variable result(this, MachineRepresentation::kTagged);

  // Check if no one installed @@hasInstance somewhere.
  GotoUnless(
      WordEqual(LoadObjectField(LoadRoot(Heap::kHasInstanceProtectorRootIndex),
                                PropertyCell::kValueOffset),
                SmiConstant(Smi::FromInt(Isolate::kProtectorValid))),
      &return_runtime);

  // Check if {callable} is a valid receiver.
  GotoIf(TaggedIsSmi(callable), &return_runtime);
  GotoUnless(IsCallableMap(LoadMap(callable)), &return_runtime);

  // Use the inline OrdinaryHasInstance directly.
  result.Bind(OrdinaryHasInstance(context, callable, object));
  Goto(&end);

  // TODO(bmeurer): Use GetPropertyStub here once available.
  Bind(&return_runtime);
  {
    result.Bind(CallRuntime(Runtime::kInstanceOf, context, object, callable));
    Goto(&end);
  }

  Bind(&end);
  return result.value();
}

compiler::Node* CodeStubAssembler::NumberInc(compiler::Node* value) {
  Variable var_result(this, MachineRepresentation::kTagged),
      var_finc_value(this, MachineRepresentation::kFloat64);
  Label if_issmi(this), if_isnotsmi(this), do_finc(this), end(this);
  Branch(TaggedIsSmi(value), &if_issmi, &if_isnotsmi);

  Bind(&if_issmi);
  {
    // Try fast Smi addition first.
    Node* one = SmiConstant(Smi::FromInt(1));
    Node* pair = IntPtrAddWithOverflow(BitcastTaggedToWord(value),
                                       BitcastTaggedToWord(one));
    Node* overflow = Projection(1, pair);

    // Check if the Smi addition overflowed.
    Label if_overflow(this), if_notoverflow(this);
    Branch(overflow, &if_overflow, &if_notoverflow);

    Bind(&if_notoverflow);
    var_result.Bind(Projection(0, pair));
    Goto(&end);

    Bind(&if_overflow);
    {
      var_finc_value.Bind(SmiToFloat64(value));
      Goto(&do_finc);
    }
  }

  Bind(&if_isnotsmi);
  {
    // Check if the value is a HeapNumber.
    CSA_ASSERT(this, IsHeapNumberMap(LoadMap(value)));

    // Load the HeapNumber value.
    var_finc_value.Bind(LoadHeapNumberValue(value));
    Goto(&do_finc);
  }

  Bind(&do_finc);
  {
    Node* finc_value = var_finc_value.value();
    Node* one = Float64Constant(1.0);
    Node* finc_result = Float64Add(finc_value, one);
    var_result.Bind(AllocateHeapNumberWithValue(finc_result));
    Goto(&end);
  }

  Bind(&end);
  return var_result.value();
}

compiler::Node* CodeStubAssembler::CreateArrayIterator(
    compiler::Node* array, compiler::Node* array_map,
    compiler::Node* array_type, compiler::Node* context, IterationKind mode) {
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

  Variable var_result(this, MachineRepresentation::kTagged);
  Variable var_map_index(this, MachineType::PointerRepresentation());
  Variable var_array_map(this, MachineRepresentation::kTagged);

  Label return_result(this);
  Label allocate_iterator(this);

  if (mode == IterationKind::kKeys) {
    // There are only two key iterator maps, branch depending on whether or not
    // the receiver is a TypedArray or not.

    Label if_istypedarray(this), if_isgeneric(this);

    Branch(Word32Equal(array_type, Int32Constant(JS_TYPED_ARRAY_TYPE)),
           &if_istypedarray, &if_isgeneric);

    Bind(&if_isgeneric);
    {
      Label if_isfast(this), if_isslow(this);
      BranchIfFastJSArray(array, context, &if_isfast, &if_isslow);

      Bind(&if_isfast);
      {
        var_map_index.Bind(
            IntPtrConstant(Context::FAST_ARRAY_KEY_ITERATOR_MAP_INDEX));
        var_array_map.Bind(array_map);
        Goto(&allocate_iterator);
      }

      Bind(&if_isslow);
      {
        var_map_index.Bind(
            IntPtrConstant(Context::GENERIC_ARRAY_KEY_ITERATOR_MAP_INDEX));
        var_array_map.Bind(UndefinedConstant());
        Goto(&allocate_iterator);
      }
    }

    Bind(&if_istypedarray);
    {
      var_map_index.Bind(
          IntPtrConstant(Context::TYPED_ARRAY_KEY_ITERATOR_MAP_INDEX));
      var_array_map.Bind(UndefinedConstant());
      Goto(&allocate_iterator);
    }
  } else {
    Label if_istypedarray(this), if_isgeneric(this);
    Branch(Word32Equal(array_type, Int32Constant(JS_TYPED_ARRAY_TYPE)),
           &if_istypedarray, &if_isgeneric);

    Bind(&if_isgeneric);
    {
      Label if_isfast(this), if_isslow(this);
      BranchIfFastJSArray(array, context, &if_isfast, &if_isslow);

      Bind(&if_isfast);
      {
        Label if_ispacked(this), if_isholey(this);
        Node* elements_kind = LoadMapElementsKind(array_map);
        Branch(IsHoleyFastElementsKind(elements_kind), &if_isholey,
               &if_ispacked);

        Bind(&if_isholey);
        {
          // Fast holey JSArrays can treat the hole as undefined if the
          // protector cell is valid, and the prototype chain is unchanged from
          // its initial state (because the protector cell is only tracked for
          // initial the Array and Object prototypes). Check these conditions
          // here, and take the slow path if any fail.
          Node* protector_cell = LoadRoot(Heap::kArrayProtectorRootIndex);
          DCHECK(isolate()->heap()->array_protector()->IsPropertyCell());
          GotoUnless(
              WordEqual(
                  LoadObjectField(protector_cell, PropertyCell::kValueOffset),
                  SmiConstant(Smi::FromInt(Isolate::kProtectorValid))),
              &if_isslow);

          Node* native_context = LoadNativeContext(context);

          Node* prototype = LoadMapPrototype(array_map);
          Node* array_prototype = LoadContextElement(
              native_context, Context::INITIAL_ARRAY_PROTOTYPE_INDEX);
          GotoUnless(WordEqual(prototype, array_prototype), &if_isslow);

          Node* map = LoadMap(prototype);
          prototype = LoadMapPrototype(map);
          Node* object_prototype = LoadContextElement(
              native_context, Context::INITIAL_OBJECT_PROTOTYPE_INDEX);
          GotoUnless(WordEqual(prototype, object_prototype), &if_isslow);

          map = LoadMap(prototype);
          prototype = LoadMapPrototype(map);
          Branch(IsNull(prototype), &if_ispacked, &if_isslow);
        }
        Bind(&if_ispacked);
        {
          Node* map_index =
              IntPtrAdd(IntPtrConstant(kBaseMapIndex + kFastIteratorOffset),
                        LoadMapElementsKind(array_map));
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

      Bind(&if_isslow);
      {
        Node* map_index = IntPtrAdd(IntPtrConstant(kBaseMapIndex),
                                    IntPtrConstant(kSlowIteratorOffset));
        var_map_index.Bind(map_index);
        var_array_map.Bind(UndefinedConstant());
        Goto(&allocate_iterator);
      }
    }

    Bind(&if_istypedarray);
    {
      Node* map_index =
          IntPtrAdd(IntPtrConstant(kBaseMapIndex - UINT8_ELEMENTS),
                    LoadMapElementsKind(array_map));
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

  Bind(&allocate_iterator);
  {
    Node* map =
        LoadFixedArrayElement(LoadNativeContext(context), var_map_index.value(),
                              0, CodeStubAssembler::INTPTR_PARAMETERS);
    var_result.Bind(AllocateJSArrayIterator(array, var_array_map.value(), map));
    Goto(&return_result);
  }

  Bind(&return_result);
  return var_result.value();
}

compiler::Node* CodeStubAssembler::AllocateJSArrayIterator(
    compiler::Node* array, compiler::Node* array_map, compiler::Node* map) {
  Node* iterator = Allocate(JSArrayIterator::kSize);
  StoreMapNoWriteBarrier(iterator, map);
  StoreObjectFieldRoot(iterator, JSArrayIterator::kPropertiesOffset,
                       Heap::kEmptyFixedArrayRootIndex);
  StoreObjectFieldRoot(iterator, JSArrayIterator::kElementsOffset,
                       Heap::kEmptyFixedArrayRootIndex);
  StoreObjectFieldNoWriteBarrier(iterator,
                                 JSArrayIterator::kIteratedObjectOffset, array);
  StoreObjectFieldNoWriteBarrier(iterator, JSArrayIterator::kNextIndexOffset,
                                 SmiConstant(Smi::FromInt(0)));
  StoreObjectFieldNoWriteBarrier(
      iterator, JSArrayIterator::kIteratedObjectMapOffset, array_map);
  return iterator;
}

compiler::Node* CodeStubAssembler::IsDetachedBuffer(compiler::Node* buffer) {
  CSA_ASSERT(this, HasInstanceType(buffer, JS_ARRAY_BUFFER_TYPE));

  Node* buffer_bit_field = LoadObjectField(
      buffer, JSArrayBuffer::kBitFieldOffset, MachineType::Uint32());
  Node* was_neutered_mask = Int32Constant(JSArrayBuffer::WasNeutered::kMask);

  return Word32NotEqual(Word32And(buffer_bit_field, was_neutered_mask),
                        Int32Constant(0));
}

CodeStubArguments::CodeStubArguments(CodeStubAssembler* assembler,
                                     compiler::Node* argc,
                                     CodeStubAssembler::ParameterMode mode)
    : assembler_(assembler),
      argc_(argc),
      arguments_(nullptr),
      fp_(assembler->LoadFramePointer()) {
  compiler::Node* offset = assembler->ElementOffsetFromIndex(
      argc_, FAST_ELEMENTS, mode,
      (StandardFrameConstants::kFixedSlotCountAboveFp - 1) * kPointerSize);
  arguments_ = assembler_->IntPtrAddFoldConstants(fp_, offset);
  if (mode == CodeStubAssembler::INTEGER_PARAMETERS) {
    argc_ = assembler->ChangeInt32ToIntPtr(argc_);
  } else if (mode == CodeStubAssembler::SMI_PARAMETERS) {
    argc_ = assembler->SmiUntag(argc_);
  }
}

compiler::Node* CodeStubArguments::GetReceiver() {
  return assembler_->Load(MachineType::AnyTagged(), arguments_,
                          assembler_->IntPtrConstant(kPointerSize));
}

compiler::Node* CodeStubArguments::AtIndex(
    compiler::Node* index, CodeStubAssembler::ParameterMode mode) {
  typedef compiler::Node Node;
  Node* negated_index = assembler_->IntPtrSubFoldConstants(
      assembler_->IntPtrOrSmiConstant(0, mode), index);
  Node* offset =
      assembler_->ElementOffsetFromIndex(negated_index, FAST_ELEMENTS, mode, 0);
  return assembler_->Load(MachineType::AnyTagged(), arguments_, offset);
}

compiler::Node* CodeStubArguments::AtIndex(int index) {
  return AtIndex(assembler_->IntPtrConstant(index));
}

void CodeStubArguments::ForEach(const CodeStubAssembler::VariableList& vars,
                                CodeStubArguments::ForEachBodyFunction body,
                                compiler::Node* first, compiler::Node* last,
                                CodeStubAssembler::ParameterMode mode) {
  assembler_->Comment("CodeStubArguments::ForEach");
  DCHECK_IMPLIES(first == nullptr || last == nullptr,
                 mode == CodeStubAssembler::INTPTR_PARAMETERS);
  if (first == nullptr) {
    first = assembler_->IntPtrOrSmiConstant(0, mode);
  }
  if (last == nullptr) {
    last = argc_;
  }
  compiler::Node* start = assembler_->IntPtrSubFoldConstants(
      arguments_,
      assembler_->ElementOffsetFromIndex(first, FAST_ELEMENTS, mode));
  compiler::Node* end = assembler_->IntPtrSubFoldConstants(
      arguments_,
      assembler_->ElementOffsetFromIndex(last, FAST_ELEMENTS, mode));
  assembler_->BuildFastLoop(
      vars, MachineType::PointerRepresentation(), start, end,
      [body](CodeStubAssembler* assembler, compiler::Node* current) {
        Node* arg = assembler->Load(MachineType::AnyTagged(), current);
        body(assembler, arg);
      },
      -kPointerSize, CodeStubAssembler::IndexAdvanceMode::kPost);
}

void CodeStubArguments::PopAndReturn(compiler::Node* value) {
  assembler_->PopAndReturn(
      assembler_->IntPtrAddFoldConstants(argc_, assembler_->IntPtrConstant(1)),
      value);
}

compiler::Node* CodeStubAssembler::IsFastElementsKind(
    compiler::Node* elements_kind) {
  return Uint32LessThanOrEqual(elements_kind,
                               Int32Constant(LAST_FAST_ELEMENTS_KIND));
}

compiler::Node* CodeStubAssembler::IsHoleyFastElementsKind(
    compiler::Node* elements_kind) {
  CSA_ASSERT(this, IsFastElementsKind(elements_kind));

  STATIC_ASSERT(FAST_HOLEY_SMI_ELEMENTS == (FAST_SMI_ELEMENTS | 1));
  STATIC_ASSERT(FAST_HOLEY_ELEMENTS == (FAST_ELEMENTS | 1));
  STATIC_ASSERT(FAST_HOLEY_DOUBLE_ELEMENTS == (FAST_DOUBLE_ELEMENTS | 1));

  // Check prototype chain if receiver does not have packed elements.
  Node* holey_elements = Word32And(elements_kind, Int32Constant(1));
  return Word32Equal(holey_elements, Int32Constant(1));
}

}  // namespace internal
}  // namespace v8
