// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/builtins/builtins-utils-gen.h"
#include "src/builtins/builtins.h"
#include "src/codegen/code-stub-assembler.h"
#include "src/ic/binary-op-assembler.h"

namespace v8 {
namespace internal {

// -----------------------------------------------------------------------------
// ES6 section 20.1 Number Objects

class NumberBuiltinsAssembler : public CodeStubAssembler {
 public:
  explicit NumberBuiltinsAssembler(compiler::CodeAssemblerState* state)
      : CodeStubAssembler(state) {}

 protected:
  template <typename Descriptor>
  void EmitBitwiseOp(Operation op) {
    TNode<Object> left = CAST(Parameter(Descriptor::kLeft));
    TNode<Object> right = CAST(Parameter(Descriptor::kRight));
    TNode<Context> context = CAST(Parameter(Descriptor::kContext));

    TVARIABLE(Word32T, var_left_word32);
    TVARIABLE(Word32T, var_right_word32);
    TVARIABLE(Object, var_left_maybe_bigint, left);
    TVARIABLE(Object, var_right_maybe_bigint);
    Label if_left_number(this), do_number_op(this);
    Label if_left_bigint(this), do_bigint_op(this);

    TaggedToWord32OrBigInt(context, left, &if_left_number, &var_left_word32,
                           &if_left_bigint, &var_left_maybe_bigint);
    BIND(&if_left_number);
    TaggedToWord32OrBigInt(context, right, &do_number_op, &var_right_word32,
                           &do_bigint_op, &var_right_maybe_bigint);
    BIND(&do_number_op);
    Return(BitwiseOp(var_left_word32.value(), var_right_word32.value(), op));

    // BigInt cases.
    BIND(&if_left_bigint);
    TaggedToNumeric(context, right, &do_bigint_op, &var_right_maybe_bigint);

    BIND(&do_bigint_op);
    Return(CallRuntime(Runtime::kBigIntBinaryOp, context,
                       var_left_maybe_bigint.value(),
                       var_right_maybe_bigint.value(), SmiConstant(op)));
  }

  template <typename Descriptor>
  void RelationalComparisonBuiltin(Operation op) {
    TNode<Object> lhs = CAST(Parameter(Descriptor::kLeft));
    TNode<Object> rhs = CAST(Parameter(Descriptor::kRight));
    TNode<Context> context = CAST(Parameter(Descriptor::kContext));

    Return(RelationalComparison(op, lhs, rhs, context));
  }

  template <typename Descriptor>
  void UnaryOp(TVariable<Object>* var_input, Label* do_smi, Label* do_double,
               TVariable<Float64T>* var_input_double, Label* do_bigint);

  template <typename Descriptor>
  void BinaryOp(Label* smis, TVariable<Object>* var_left,
                TVariable<Object>* var_right, Label* doubles,
                TVariable<Float64T>* var_left_double,
                TVariable<Float64T>* var_right_double, Label* bigints);
};

// ES6 #sec-number.isfinite
TF_BUILTIN(NumberIsFinite, CodeStubAssembler) {
  TNode<Object> number = CAST(Parameter(Descriptor::kNumber));

  Label return_true(this), return_false(this);

  // Check if {number} is a Smi.
  GotoIf(TaggedIsSmi(number), &return_true);

  // Check if {number} is a HeapNumber.
  TNode<HeapObject> number_heap_object = CAST(number);
  GotoIfNot(IsHeapNumber(number_heap_object), &return_false);

  // Check if {number} contains a finite, non-NaN value.
  TNode<Float64T> number_value = LoadHeapNumberValue(number_heap_object);
  BranchIfFloat64IsNaN(Float64Sub(number_value, number_value), &return_false,
                       &return_true);

  BIND(&return_true);
  Return(TrueConstant());

  BIND(&return_false);
  Return(FalseConstant());
}

TF_BUILTIN(AllocateHeapNumber, CodeStubAssembler) {
  TNode<HeapNumber> result = AllocateHeapNumber();
  Return(result);
}

// ES6 #sec-number.isinteger
TF_BUILTIN(NumberIsInteger, CodeStubAssembler) {
  TNode<Object> number = CAST(Parameter(Descriptor::kNumber));
  Return(SelectBooleanConstant(IsInteger(number)));
}

// ES6 #sec-number.isnan
TF_BUILTIN(NumberIsNaN, CodeStubAssembler) {
  TNode<Object> number = CAST(Parameter(Descriptor::kNumber));

  Label return_true(this), return_false(this);

  // Check if {number} is a Smi.
  GotoIf(TaggedIsSmi(number), &return_false);

  // Check if {number} is a HeapNumber.
  TNode<HeapObject> number_heap_object = CAST(number);
  GotoIfNot(IsHeapNumber(number_heap_object), &return_false);

  // Check if {number} contains a NaN value.
  TNode<Float64T> number_value = LoadHeapNumberValue(number_heap_object);
  BranchIfFloat64IsNaN(number_value, &return_true, &return_false);

  BIND(&return_true);
  Return(TrueConstant());

  BIND(&return_false);
  Return(FalseConstant());
}

// ES6 #sec-number.issafeinteger
TF_BUILTIN(NumberIsSafeInteger, CodeStubAssembler) {
  TNode<Object> number = CAST(Parameter(Descriptor::kNumber));
  Return(SelectBooleanConstant(IsSafeInteger(number)));
}

// ES6 #sec-number.parsefloat
TF_BUILTIN(NumberParseFloat, CodeStubAssembler) {
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));

  // We might need to loop once for ToString conversion.
  TVARIABLE(Object, var_input, CAST(Parameter(Descriptor::kString)));
  Label loop(this, &var_input);
  Goto(&loop);
  BIND(&loop);
  {
    // Load the current {input} value.
    TNode<Object> input = var_input.value();

    // Check if the {input} is a HeapObject or a Smi.
    Label if_inputissmi(this), if_inputisnotsmi(this);
    Branch(TaggedIsSmi(input), &if_inputissmi, &if_inputisnotsmi);

    BIND(&if_inputissmi);
    {
      // The {input} is already a Number, no need to do anything.
      Return(input);
    }

    BIND(&if_inputisnotsmi);
    {
      // The {input} is a HeapObject, check if it's already a String.
      TNode<HeapObject> input_heap_object = CAST(input);
      Label if_inputisstring(this), if_inputisnotstring(this);
      TNode<Map> input_map = LoadMap(input_heap_object);
      TNode<Uint16T> input_instance_type = LoadMapInstanceType(input_map);
      Branch(IsStringInstanceType(input_instance_type), &if_inputisstring,
             &if_inputisnotstring);

      BIND(&if_inputisstring);
      {
        // The {input} is already a String, check if {input} contains
        // a cached array index.
        Label if_inputcached(this), if_inputnotcached(this);
        TNode<Uint32T> input_hash = LoadNameHashField(CAST(input));
        Branch(IsClearWord32(input_hash,
                             Name::kDoesNotContainCachedArrayIndexMask),
               &if_inputcached, &if_inputnotcached);

        BIND(&if_inputcached);
        {
          // Just return the {input}s cached array index.
          TNode<UintPtrT> input_array_index =
              DecodeWordFromWord32<String::ArrayIndexValueBits>(input_hash);
          Return(SmiTag(Signed(input_array_index)));
        }

        BIND(&if_inputnotcached);
        {
          // Need to fall back to the runtime to convert {input} to double.
          Return(CallRuntime(Runtime::kStringParseFloat, context, input));
        }
      }

      BIND(&if_inputisnotstring);
      {
        // The {input} is neither a String nor a Smi, check for HeapNumber.
        Label if_inputisnumber(this),
            if_inputisnotnumber(this, Label::kDeferred);
        Branch(IsHeapNumberMap(input_map), &if_inputisnumber,
               &if_inputisnotnumber);

        BIND(&if_inputisnumber);
        {
          // The {input} is already a Number, take care of -0.
          Label if_inputiszero(this), if_inputisnotzero(this);
          TNode<Float64T> input_value = LoadHeapNumberValue(input_heap_object);
          Branch(Float64Equal(input_value, Float64Constant(0.0)),
                 &if_inputiszero, &if_inputisnotzero);

          BIND(&if_inputiszero);
          Return(SmiConstant(0));

          BIND(&if_inputisnotzero);
          Return(input);
        }

        BIND(&if_inputisnotnumber);
        {
          // Need to convert the {input} to String first.
          // TODO(bmeurer): This could be more efficient if necessary.
          var_input = CallBuiltin(Builtins::kToString, context, input);
          Goto(&loop);
        }
      }
    }
  }
}

// ES6 #sec-number.parseint
TF_BUILTIN(ParseInt, CodeStubAssembler) {
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));
  TNode<Object> input = CAST(Parameter(Descriptor::kString));
  TNode<Object> radix = CAST(Parameter(Descriptor::kRadix));

  // Check if {radix} is treated as 10 (i.e. undefined, 0 or 10).
  Label if_radix10(this), if_generic(this, Label::kDeferred);
  GotoIf(IsUndefined(radix), &if_radix10);
  GotoIf(TaggedEqual(radix, SmiConstant(10)), &if_radix10);
  GotoIf(TaggedEqual(radix, SmiConstant(0)), &if_radix10);
  Goto(&if_generic);

  BIND(&if_radix10);
  {
    // Check if we can avoid the ToString conversion on {input}.
    Label if_inputissmi(this), if_inputisheapnumber(this),
        if_inputisstring(this);
    GotoIf(TaggedIsSmi(input), &if_inputissmi);
    TNode<Map> input_map = LoadMap(CAST(input));
    GotoIf(IsHeapNumberMap(input_map), &if_inputisheapnumber);
    TNode<Uint16T> input_instance_type = LoadMapInstanceType(input_map);
    Branch(IsStringInstanceType(input_instance_type), &if_inputisstring,
           &if_generic);

    BIND(&if_inputissmi);
    {
      // Just return the {input}.
      Return(input);
    }

    BIND(&if_inputisheapnumber);
    {
      // Check if the {input} value is in Signed32 range.
      Label if_inputissigned32(this);
      TNode<Float64T> input_value = LoadHeapNumberValue(CAST(input));
      TNode<Int32T> input_value32 =
          Signed(TruncateFloat64ToWord32(input_value));
      GotoIf(Float64Equal(input_value, ChangeInt32ToFloat64(input_value32)),
             &if_inputissigned32);

      // Check if the absolute {input} value is in the [1,1<<31[ range.
      // Take the generic path for the range [0,1[ because the result
      // could be -0.
      TNode<Float64T> input_value_abs = Float64Abs(input_value);

      GotoIfNot(Float64LessThan(input_value_abs, Float64Constant(1u << 31)),
                &if_generic);
      Branch(Float64LessThanOrEqual(Float64Constant(1), input_value_abs),
             &if_inputissigned32, &if_generic);

      // Return the truncated int32 value, and return the tagged result.
      BIND(&if_inputissigned32);
      TNode<Number> result = ChangeInt32ToTagged(input_value32);
      Return(result);
    }

    BIND(&if_inputisstring);
    {
      // Check if the String {input} has a cached array index.
      TNode<Uint32T> input_hash = LoadNameHashField(CAST(input));
      GotoIf(IsSetWord32(input_hash, Name::kDoesNotContainCachedArrayIndexMask),
             &if_generic);

      // Return the cached array index as result.
      TNode<UintPtrT> input_index =
          DecodeWordFromWord32<String::ArrayIndexValueBits>(input_hash);
      TNode<Smi> result = SmiTag(Signed(input_index));
      Return(result);
    }
  }

  BIND(&if_generic);
  {
    TNode<Object> result =
        CallRuntime(Runtime::kStringParseInt, context, input, radix);
    Return(result);
  }
}

// ES6 #sec-number.parseint
TF_BUILTIN(NumberParseInt, CodeStubAssembler) {
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));
  TNode<Object> input = CAST(Parameter(Descriptor::kString));
  TNode<Object> radix = CAST(Parameter(Descriptor::kRadix));
  Return(CallBuiltin(Builtins::kParseInt, context, input, radix));
}

// ES6 #sec-number.prototype.valueof
TF_BUILTIN(NumberPrototypeValueOf, CodeStubAssembler) {
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));
  TNode<Object> receiver = CAST(Parameter(Descriptor::kReceiver));

  TNode<Object> result = ToThisValue(context, receiver, PrimitiveType::kNumber,
                                     "Number.prototype.valueOf");
  Return(result);
}

class AddStubAssembler : public CodeStubAssembler {
 public:
  explicit AddStubAssembler(compiler::CodeAssemblerState* state)
      : CodeStubAssembler(state) {}

 protected:
  TNode<Object> ConvertReceiver(TNode<JSReceiver> js_receiver,
                                TNode<Context> context) {
    // Call ToPrimitive explicitly without hint (whereas ToNumber
    // would pass a "number" hint).
    Callable callable = CodeFactory::NonPrimitiveToPrimitive(isolate());
    return CallStub(callable, context, js_receiver);
  }

  void ConvertNonReceiverAndLoop(TVariable<Object>* var_value, Label* loop,
                                 TNode<Context> context) {
    *var_value =
        CallBuiltin(Builtins::kNonNumberToNumeric, context, var_value->value());
    Goto(loop);
  }

  void ConvertAndLoop(TVariable<Object>* var_value,
                      TNode<Uint16T> instance_type, Label* loop,
                      TNode<Context> context) {
    Label is_not_receiver(this, Label::kDeferred);
    GotoIfNot(IsJSReceiverInstanceType(instance_type), &is_not_receiver);

    *var_value = ConvertReceiver(CAST(var_value->value()), context);
    Goto(loop);

    BIND(&is_not_receiver);
    ConvertNonReceiverAndLoop(var_value, loop, context);
  }
};

TF_BUILTIN(Add, AddStubAssembler) {
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));
  TVARIABLE(Object, var_left, CAST(Parameter(Descriptor::kLeft)));
  TVARIABLE(Object, var_right, CAST(Parameter(Descriptor::kRight)));

  // Shared entry for floating point addition.
  Label do_double_add(this);
  TVARIABLE(Float64T, var_left_double);
  TVARIABLE(Float64T, var_right_double);

  // We might need to loop several times due to ToPrimitive, ToString and/or
  // ToNumeric conversions.
  Label loop(this, {&var_left, &var_right}),
      string_add_convert_left(this, Label::kDeferred),
      string_add_convert_right(this, Label::kDeferred),
      do_bigint_add(this, Label::kDeferred);
  Goto(&loop);
  BIND(&loop);
  {
    TNode<Object> left = var_left.value();
    TNode<Object> right = var_right.value();

    Label if_left_smi(this), if_left_heapobject(this);
    Branch(TaggedIsSmi(left), &if_left_smi, &if_left_heapobject);

    BIND(&if_left_smi);
    {
      Label if_right_smi(this), if_right_heapobject(this);
      Branch(TaggedIsSmi(right), &if_right_smi, &if_right_heapobject);

      BIND(&if_right_smi);
      {
        Label if_overflow(this);
        TNode<Smi> left_smi = CAST(left);
        TNode<Smi> right_smi = CAST(right);
        TNode<Smi> result = TrySmiAdd(left_smi, right_smi, &if_overflow);
        Return(result);

        BIND(&if_overflow);
        {
          var_left_double = SmiToFloat64(left_smi);
          var_right_double = SmiToFloat64(right_smi);
          Goto(&do_double_add);
        }
      }  // if_right_smi

      BIND(&if_right_heapobject);
      {
        TNode<HeapObject> right_heap_object = CAST(right);
        TNode<Map> right_map = LoadMap(right_heap_object);

        Label if_right_not_number(this, Label::kDeferred);
        GotoIfNot(IsHeapNumberMap(right_map), &if_right_not_number);

        // {right} is a HeapNumber.
        var_left_double = SmiToFloat64(CAST(left));
        var_right_double = LoadHeapNumberValue(right_heap_object);
        Goto(&do_double_add);

        BIND(&if_right_not_number);
        {
          TNode<Uint16T> right_instance_type = LoadMapInstanceType(right_map);
          GotoIf(IsStringInstanceType(right_instance_type),
                 &string_add_convert_left);
          GotoIf(IsBigIntInstanceType(right_instance_type), &do_bigint_add);
          ConvertAndLoop(&var_right, right_instance_type, &loop, context);
        }
      }  // if_right_heapobject
    }    // if_left_smi

    BIND(&if_left_heapobject);
    {
      TNode<HeapObject> left_heap_object = CAST(left);
      TNode<Map> left_map = LoadMap(left_heap_object);
      Label if_right_smi(this), if_right_heapobject(this);
      Branch(TaggedIsSmi(right), &if_right_smi, &if_right_heapobject);

      BIND(&if_right_smi);
      {
        Label if_left_not_number(this, Label::kDeferred);
        GotoIfNot(IsHeapNumberMap(left_map), &if_left_not_number);

        // {left} is a HeapNumber, {right} is a Smi.
        var_left_double = LoadHeapNumberValue(left_heap_object);
        var_right_double = SmiToFloat64(CAST(right));
        Goto(&do_double_add);

        BIND(&if_left_not_number);
        {
          TNode<Uint16T> left_instance_type = LoadMapInstanceType(left_map);
          GotoIf(IsStringInstanceType(left_instance_type),
                 &string_add_convert_right);
          GotoIf(IsBigIntInstanceType(left_instance_type), &do_bigint_add);
          // {left} is neither a Numeric nor a String, and {right} is a Smi.
          ConvertAndLoop(&var_left, left_instance_type, &loop, context);
        }
      }  // if_right_smi

      BIND(&if_right_heapobject);
      {
        TNode<HeapObject> right_heap_object = CAST(right);
        TNode<Map> right_map = LoadMap(right_heap_object);

        Label if_left_number(this), if_left_not_number(this, Label::kDeferred);
        Branch(IsHeapNumberMap(left_map), &if_left_number, &if_left_not_number);

        BIND(&if_left_number);
        {
          Label if_right_not_number(this, Label::kDeferred);
          GotoIfNot(IsHeapNumberMap(right_map), &if_right_not_number);

          // Both {left} and {right} are HeapNumbers.
          var_left_double = LoadHeapNumberValue(CAST(left));
          var_right_double = LoadHeapNumberValue(right_heap_object);
          Goto(&do_double_add);

          BIND(&if_right_not_number);
          {
            TNode<Uint16T> right_instance_type = LoadMapInstanceType(right_map);
            GotoIf(IsStringInstanceType(right_instance_type),
                   &string_add_convert_left);
            GotoIf(IsBigIntInstanceType(right_instance_type), &do_bigint_add);
            // {left} is a HeapNumber, {right} is neither Number nor String.
            ConvertAndLoop(&var_right, right_instance_type, &loop, context);
          }
        }  // if_left_number

        BIND(&if_left_not_number);
        {
          Label if_left_bigint(this);
          TNode<Uint16T> left_instance_type = LoadMapInstanceType(left_map);
          GotoIf(IsStringInstanceType(left_instance_type),
                 &string_add_convert_right);
          TNode<Uint16T> right_instance_type = LoadMapInstanceType(right_map);
          GotoIf(IsStringInstanceType(right_instance_type),
                 &string_add_convert_left);
          GotoIf(IsBigIntInstanceType(left_instance_type), &if_left_bigint);
          Label if_left_not_receiver(this, Label::kDeferred);
          Label if_right_not_receiver(this, Label::kDeferred);
          GotoIfNot(IsJSReceiverInstanceType(left_instance_type),
                    &if_left_not_receiver);
          // {left} is a JSReceiver, convert it first.
          var_left = ConvertReceiver(CAST(var_left.value()), context);
          Goto(&loop);

          BIND(&if_left_bigint);
          {
            // {right} is a HeapObject, but not a String. Jump to
            // {do_bigint_add} if {right} is already a Numeric.
            GotoIf(IsBigIntInstanceType(right_instance_type), &do_bigint_add);
            GotoIf(IsHeapNumberMap(right_map), &do_bigint_add);
            ConvertAndLoop(&var_right, right_instance_type, &loop, context);
          }

          BIND(&if_left_not_receiver);
          GotoIfNot(IsJSReceiverInstanceType(right_instance_type),
                    &if_right_not_receiver);
          // {left} is a Primitive, but {right} is a JSReceiver, so convert
          // {right} with priority.
          var_right = ConvertReceiver(CAST(var_right.value()), context);
          Goto(&loop);

          BIND(&if_right_not_receiver);
          // Neither {left} nor {right} are JSReceivers.
          ConvertNonReceiverAndLoop(&var_left, &loop, context);
        }
      }  // if_right_heapobject
    }    // if_left_heapobject
  }
  BIND(&string_add_convert_left);
  {
    // Convert {left} to a String and concatenate it with the String {right}.
    TailCallBuiltin(Builtins::kStringAddConvertLeft, context, var_left.value(),
                    var_right.value());
  }

  BIND(&string_add_convert_right);
  {
    // Convert {right} to a String and concatenate it with the String {left}.
    TailCallBuiltin(Builtins::kStringAddConvertRight, context, var_left.value(),
                    var_right.value());
  }

  BIND(&do_bigint_add);
  {
    TailCallBuiltin(Builtins::kBigIntAdd, context, var_left.value(),
                    var_right.value());
  }

  BIND(&do_double_add);
  {
    TNode<Float64T> value =
        Float64Add(var_left_double.value(), var_right_double.value());
    Return(AllocateHeapNumberWithValue(value));
  }
}

template <typename Descriptor>
void NumberBuiltinsAssembler::UnaryOp(TVariable<Object>* var_input,
                                      Label* do_smi, Label* do_double,
                                      TVariable<Float64T>* var_input_double,
                                      Label* do_bigint) {
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));
  *var_input = CAST(Parameter(Descriptor::kValue));

  // We might need to loop for ToNumeric conversion.
  Label loop(this, {var_input});
  Goto(&loop);
  BIND(&loop);
  TNode<Object> input = var_input->value();

  Label not_number(this);
  GotoIf(TaggedIsSmi(input), do_smi);
  TNode<HeapObject> input_heap_object = CAST(input);
  GotoIfNot(IsHeapNumber(input_heap_object), &not_number);
  if (var_input_double != nullptr) {
    *var_input_double = LoadHeapNumberValue(input_heap_object);
  }
  Goto(do_double);

  BIND(&not_number);
  GotoIf(IsBigInt(input_heap_object), do_bigint);
  *var_input = CallBuiltin(Builtins::kNonNumberToNumeric, context, input);
  Goto(&loop);
}

template <typename Descriptor>
void NumberBuiltinsAssembler::BinaryOp(Label* smis, TVariable<Object>* var_left,
                                       TVariable<Object>* var_right,
                                       Label* doubles,
                                       TVariable<Float64T>* var_left_double,
                                       TVariable<Float64T>* var_right_double,
                                       Label* bigints) {
  DCHECK_EQ(var_left_double == nullptr, var_right_double == nullptr);

  TNode<Context> context = CAST(Parameter(Descriptor::kContext));
  *var_left = CAST(Parameter(Descriptor::kLeft));
  *var_right = CAST(Parameter(Descriptor::kRight));

  // We might need to loop for ToNumeric conversions.
  Label loop(this, {var_left, var_right});
  Goto(&loop);
  BIND(&loop);

  Label left_not_smi(this), right_not_smi(this);
  Label left_not_number(this), right_not_number(this);
  GotoIfNot(TaggedIsSmi(var_left->value()), &left_not_smi);
  GotoIf(TaggedIsSmi(var_right->value()), smis);

  // At this point, var_left is a Smi but var_right is not.
  TNode<Smi> var_left_smi = CAST(var_left->value());
  TNode<HeapObject> var_right_heap_object = CAST(var_right->value());
  GotoIfNot(IsHeapNumber(var_right_heap_object), &right_not_number);
  if (var_left_double != nullptr) {
    *var_left_double = SmiToFloat64(var_left_smi);
    *var_right_double = LoadHeapNumberValue(var_right_heap_object);
  }
  Goto(doubles);

  BIND(&left_not_smi);
  {
    TNode<HeapObject> var_left_heap_object = CAST(var_left->value());
    GotoIfNot(IsHeapNumber(var_left_heap_object), &left_not_number);
    GotoIfNot(TaggedIsSmi(var_right->value()), &right_not_smi);

    // At this point, var_left is a HeapNumber and var_right is a Smi.
    if (var_left_double != nullptr) {
      *var_left_double = LoadHeapNumberValue(var_left_heap_object);
      *var_right_double = SmiToFloat64(CAST(var_right->value()));
    }
    Goto(doubles);
  }

  BIND(&right_not_smi);
  {
    TNode<HeapObject> var_right_heap_object = CAST(var_right->value());
    GotoIfNot(IsHeapNumber(var_right_heap_object), &right_not_number);
    if (var_left_double != nullptr) {
      *var_left_double = LoadHeapNumberValue(CAST(var_left->value()));
      *var_right_double = LoadHeapNumberValue(var_right_heap_object);
    }
    Goto(doubles);
  }

  BIND(&left_not_number);
  {
    Label left_bigint(this);
    GotoIf(IsBigInt(CAST(var_left->value())), &left_bigint);
    *var_left =
        CallBuiltin(Builtins::kNonNumberToNumeric, context, var_left->value());
    Goto(&loop);

    BIND(&left_bigint);
    {
      // Jump to {bigints} if {var_right} is already a Numeric.
      GotoIf(TaggedIsSmi(var_right->value()), bigints);
      TNode<HeapObject> var_right_heap_object = CAST(var_right->value());
      GotoIf(IsBigInt(var_right_heap_object), bigints);
      GotoIf(IsHeapNumber(var_right_heap_object), bigints);
      *var_right = CallBuiltin(Builtins::kNonNumberToNumeric, context,
                               var_right->value());
      Goto(&loop);
    }
  }

  BIND(&right_not_number);
  {
    GotoIf(IsBigInt(CAST(var_right->value())), bigints);
    *var_right =
        CallBuiltin(Builtins::kNonNumberToNumeric, context, var_right->value());
    Goto(&loop);
  }
}

TF_BUILTIN(Subtract, NumberBuiltinsAssembler) {
  TVARIABLE(Object, var_left);
  TVARIABLE(Object, var_right);
  TVARIABLE(Float64T, var_left_double);
  TVARIABLE(Float64T, var_right_double);
  Label do_smi_sub(this), do_double_sub(this), do_bigint_sub(this);

  BinaryOp<Descriptor>(&do_smi_sub, &var_left, &var_right, &do_double_sub,
                       &var_left_double, &var_right_double, &do_bigint_sub);

  BIND(&do_smi_sub);
  {
    Label if_overflow(this);
    TNode<Smi> var_left_smi = CAST(var_left.value());
    TNode<Smi> var_right_smi = CAST(var_right.value());
    TNode<Smi> result = TrySmiSub(var_left_smi, var_right_smi, &if_overflow);
    Return(result);

    BIND(&if_overflow);
    {
      var_left_double = SmiToFloat64(var_left_smi);
      var_right_double = SmiToFloat64(var_right_smi);
      Goto(&do_double_sub);
    }
  }

  BIND(&do_double_sub);
  {
    TNode<Float64T> value =
        Float64Sub(var_left_double.value(), var_right_double.value());
    Return(AllocateHeapNumberWithValue(value));
  }

  BIND(&do_bigint_sub);
  {
    TNode<Context> context = CAST(Parameter(Descriptor::kContext));
    TailCallBuiltin(Builtins::kBigIntSubtract, context, var_left.value(),
                    var_right.value());
  }
}

TF_BUILTIN(BitwiseNot, NumberBuiltinsAssembler) {
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));
  TVARIABLE(Object, var_input);
  Label do_number(this), do_bigint(this);

  UnaryOp<Descriptor>(&var_input, &do_number, &do_number, nullptr, &do_bigint);

  BIND(&do_number);
  {
    TailCallBuiltin(Builtins::kBitwiseXor, context, var_input.value(),
                    SmiConstant(-1));
  }

  BIND(&do_bigint);
  {
    Return(CallRuntime(Runtime::kBigIntUnaryOp, context, var_input.value(),
                       SmiConstant(Operation::kBitwiseNot)));
  }
}

TF_BUILTIN(Decrement, NumberBuiltinsAssembler) {
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));
  TVARIABLE(Object, var_input);
  Label do_number(this), do_bigint(this);

  UnaryOp<Descriptor>(&var_input, &do_number, &do_number, nullptr, &do_bigint);

  BIND(&do_number);
  {
    TailCallBuiltin(Builtins::kSubtract, context, var_input.value(),
                    SmiConstant(1));
  }

  BIND(&do_bigint);
  {
    Return(CallRuntime(Runtime::kBigIntUnaryOp, context, var_input.value(),
                       SmiConstant(Operation::kDecrement)));
  }
}

TF_BUILTIN(Increment, NumberBuiltinsAssembler) {
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));
  TVARIABLE(Object, var_input);
  Label do_number(this), do_bigint(this);

  UnaryOp<Descriptor>(&var_input, &do_number, &do_number, nullptr, &do_bigint);

  BIND(&do_number);
  {
    TailCallBuiltin(Builtins::kAdd, context, var_input.value(), SmiConstant(1));
  }

  BIND(&do_bigint);
  {
    Return(CallRuntime(Runtime::kBigIntUnaryOp, context, var_input.value(),
                       SmiConstant(Operation::kIncrement)));
  }
}

TF_BUILTIN(Negate, NumberBuiltinsAssembler) {
  TVARIABLE(Object, var_input);
  TVARIABLE(Float64T, var_input_double);
  Label do_smi(this), do_double(this), do_bigint(this);

  UnaryOp<Descriptor>(&var_input, &do_smi, &do_double, &var_input_double,
                      &do_bigint);

  BIND(&do_smi);
  { Return(SmiMul(CAST(var_input.value()), SmiConstant(-1))); }

  BIND(&do_double);
  {
    TNode<Float64T> value =
        Float64Mul(var_input_double.value(), Float64Constant(-1));
    Return(AllocateHeapNumberWithValue(value));
  }

  BIND(&do_bigint);
  {
    TNode<Context> context = CAST(Parameter(Descriptor::kContext));
    Return(CallRuntime(Runtime::kBigIntUnaryOp, context, var_input.value(),
                       SmiConstant(Operation::kNegate)));
  }
}

TF_BUILTIN(Multiply, NumberBuiltinsAssembler) {
  TVARIABLE(Object, var_left);
  TVARIABLE(Object, var_right);
  TVARIABLE(Float64T, var_left_double);
  TVARIABLE(Float64T, var_right_double);
  Label do_smi_mul(this), do_double_mul(this), do_bigint_mul(this);

  BinaryOp<Descriptor>(&do_smi_mul, &var_left, &var_right, &do_double_mul,
                       &var_left_double, &var_right_double, &do_bigint_mul);

  BIND(&do_smi_mul);
  // The result is not necessarily a smi, in case of overflow.
  Return(SmiMul(CAST(var_left.value()), CAST(var_right.value())));

  BIND(&do_double_mul);
  TNode<Float64T> value =
      Float64Mul(var_left_double.value(), var_right_double.value());
  Return(AllocateHeapNumberWithValue(value));

  BIND(&do_bigint_mul);
  {
    TNode<Context> context = CAST(Parameter(Descriptor::kContext));
    Return(CallRuntime(Runtime::kBigIntBinaryOp, context, var_left.value(),
                       var_right.value(), SmiConstant(Operation::kMultiply)));
  }
}

TF_BUILTIN(Divide, NumberBuiltinsAssembler) {
  TVARIABLE(Object, var_left);
  TVARIABLE(Object, var_right);
  TVARIABLE(Float64T, var_left_double);
  TVARIABLE(Float64T, var_right_double);
  Label do_smi_div(this), do_double_div(this), do_bigint_div(this);

  BinaryOp<Descriptor>(&do_smi_div, &var_left, &var_right, &do_double_div,
                       &var_left_double, &var_right_double, &do_bigint_div);

  BIND(&do_smi_div);
  {
    // TODO(jkummerow): Consider just always doing a double division.
    Label bailout(this);
    TNode<Smi> dividend = CAST(var_left.value());
    TNode<Smi> divisor = CAST(var_right.value());

    // Do floating point division if {divisor} is zero.
    GotoIf(SmiEqual(divisor, SmiConstant(0)), &bailout);

    // Do floating point division if {dividend} is zero and {divisor} is
    // negative.
    Label dividend_is_zero(this), dividend_is_not_zero(this);
    Branch(SmiEqual(dividend, SmiConstant(0)), &dividend_is_zero,
           &dividend_is_not_zero);

    BIND(&dividend_is_zero);
    {
      GotoIf(SmiLessThan(divisor, SmiConstant(0)), &bailout);
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
             &bailout);
      Goto(&divisor_is_not_minus_one);
    }
    BIND(&divisor_is_not_minus_one);

    // TODO(epertoso): consider adding a machine instruction that returns
    // both the result and the remainder.
    TNode<Int32T> untagged_result =
        Int32Div(untagged_dividend, untagged_divisor);
    TNode<Int32T> truncated = Int32Mul(untagged_result, untagged_divisor);
    // Do floating point division if the remainder is not 0.
    GotoIf(Word32NotEqual(untagged_dividend, truncated), &bailout);
    Return(SmiFromInt32(untagged_result));

    // Bailout: convert {dividend} and {divisor} to double and do double
    // division.
    BIND(&bailout);
    {
      var_left_double = SmiToFloat64(dividend);
      var_right_double = SmiToFloat64(divisor);
      Goto(&do_double_div);
    }
  }

  BIND(&do_double_div);
  {
    TNode<Float64T> value =
        Float64Div(var_left_double.value(), var_right_double.value());
    Return(AllocateHeapNumberWithValue(value));
  }

  BIND(&do_bigint_div);
  {
    TNode<Context> context = CAST(Parameter(Descriptor::kContext));
    Return(CallRuntime(Runtime::kBigIntBinaryOp, context, var_left.value(),
                       var_right.value(), SmiConstant(Operation::kDivide)));
  }
}

TF_BUILTIN(Modulus, NumberBuiltinsAssembler) {
  TVARIABLE(Object, var_left);
  TVARIABLE(Object, var_right);
  TVARIABLE(Float64T, var_left_double);
  TVARIABLE(Float64T, var_right_double);
  Label do_smi_mod(this), do_double_mod(this), do_bigint_mod(this);

  BinaryOp<Descriptor>(&do_smi_mod, &var_left, &var_right, &do_double_mod,
                       &var_left_double, &var_right_double, &do_bigint_mod);

  BIND(&do_smi_mod);
  Return(SmiMod(CAST(var_left.value()), CAST(var_right.value())));

  BIND(&do_double_mod);
  TNode<Float64T> value =
      Float64Mod(var_left_double.value(), var_right_double.value());
  Return(AllocateHeapNumberWithValue(value));

  BIND(&do_bigint_mod);
  {
    TNode<Context> context = CAST(Parameter(Descriptor::kContext));
    Return(CallRuntime(Runtime::kBigIntBinaryOp, context, var_left.value(),
                       var_right.value(), SmiConstant(Operation::kModulus)));
  }
}

TF_BUILTIN(Exponentiate, NumberBuiltinsAssembler) {
  TVARIABLE(Object, var_left);
  TVARIABLE(Object, var_right);
  Label do_number_exp(this), do_bigint_exp(this);
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));

  BinaryOp<Descriptor>(&do_number_exp, &var_left, &var_right, &do_number_exp,
                       nullptr, nullptr, &do_bigint_exp);

  BIND(&do_number_exp);
  { Return(MathPowImpl(context, var_left.value(), var_right.value())); }

  BIND(&do_bigint_exp);
  Return(CallRuntime(Runtime::kBigIntBinaryOp, context, var_left.value(),
                     var_right.value(), SmiConstant(Operation::kExponentiate)));
}

TF_BUILTIN(ShiftLeft, NumberBuiltinsAssembler) {
  EmitBitwiseOp<Descriptor>(Operation::kShiftLeft);
}

TF_BUILTIN(ShiftRight, NumberBuiltinsAssembler) {
  EmitBitwiseOp<Descriptor>(Operation::kShiftRight);
}

TF_BUILTIN(ShiftRightLogical, NumberBuiltinsAssembler) {
  EmitBitwiseOp<Descriptor>(Operation::kShiftRightLogical);
}

TF_BUILTIN(BitwiseAnd, NumberBuiltinsAssembler) {
  EmitBitwiseOp<Descriptor>(Operation::kBitwiseAnd);
}

TF_BUILTIN(BitwiseOr, NumberBuiltinsAssembler) {
  EmitBitwiseOp<Descriptor>(Operation::kBitwiseOr);
}

TF_BUILTIN(BitwiseXor, NumberBuiltinsAssembler) {
  EmitBitwiseOp<Descriptor>(Operation::kBitwiseXor);
}

TF_BUILTIN(LessThan, NumberBuiltinsAssembler) {
  RelationalComparisonBuiltin<Descriptor>(Operation::kLessThan);
}

TF_BUILTIN(LessThanOrEqual, NumberBuiltinsAssembler) {
  RelationalComparisonBuiltin<Descriptor>(Operation::kLessThanOrEqual);
}

TF_BUILTIN(GreaterThan, NumberBuiltinsAssembler) {
  RelationalComparisonBuiltin<Descriptor>(Operation::kGreaterThan);
}

TF_BUILTIN(GreaterThanOrEqual, NumberBuiltinsAssembler) {
  RelationalComparisonBuiltin<Descriptor>(Operation::kGreaterThanOrEqual);
}

TF_BUILTIN(Equal, CodeStubAssembler) {
  TNode<Object> lhs = CAST(Parameter(Descriptor::kLeft));
  TNode<Object> rhs = CAST(Parameter(Descriptor::kRight));
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));

  Return(Equal(lhs, rhs, context));
}

TF_BUILTIN(StrictEqual, CodeStubAssembler) {
  TNode<Object> lhs = CAST(Parameter(Descriptor::kLeft));
  TNode<Object> rhs = CAST(Parameter(Descriptor::kRight));

  Return(StrictEqual(lhs, rhs));
}

}  // namespace internal
}  // namespace v8
