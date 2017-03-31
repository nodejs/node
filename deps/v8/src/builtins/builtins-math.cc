// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/builtins/builtins-utils.h"
#include "src/builtins/builtins.h"
#include "src/code-factory.h"
#include "src/code-stub-assembler.h"

namespace v8 {
namespace internal {

// -----------------------------------------------------------------------------
// ES6 section 20.2.2 Function Properties of the Math Object

class MathBuiltinsAssembler : public CodeStubAssembler {
 public:
  explicit MathBuiltinsAssembler(compiler::CodeAssemblerState* state)
      : CodeStubAssembler(state) {}

 protected:
  void MathRoundingOperation(Node* (CodeStubAssembler::*float64op)(Node*));
  void MathUnaryOperation(Node* (CodeStubAssembler::*float64op)(Node*));
};

// ES6 section - 20.2.2.1 Math.abs ( x )
TF_BUILTIN(MathAbs, CodeStubAssembler) {
  Node* context = Parameter(4);

  // We might need to loop once for ToNumber conversion.
  Variable var_x(this, MachineRepresentation::kTagged);
  Label loop(this, &var_x);
  var_x.Bind(Parameter(1));
  Goto(&loop);
  Bind(&loop);
  {
    // Load the current {x} value.
    Node* x = var_x.value();

    // Check if {x} is a Smi or a HeapObject.
    Label if_xissmi(this), if_xisnotsmi(this);
    Branch(TaggedIsSmi(x), &if_xissmi, &if_xisnotsmi);

    Bind(&if_xissmi);
    {
      // Check if {x} is already positive.
      Label if_xispositive(this), if_xisnotpositive(this);
      BranchIfSmiLessThanOrEqual(SmiConstant(Smi::FromInt(0)), x,
                                 &if_xispositive, &if_xisnotpositive);

      Bind(&if_xispositive);
      {
        // Just return the input {x}.
        Return(x);
      }

      Bind(&if_xisnotpositive);
      {
        // Try to negate the {x} value.
        Node* pair =
            IntPtrSubWithOverflow(IntPtrConstant(0), BitcastTaggedToWord(x));
        Node* overflow = Projection(1, pair);
        Label if_overflow(this, Label::kDeferred), if_notoverflow(this);
        Branch(overflow, &if_overflow, &if_notoverflow);

        Bind(&if_notoverflow);
        {
          // There is a Smi representation for negated {x}.
          Node* result = Projection(0, pair);
          Return(BitcastWordToTagged(result));
        }

        Bind(&if_overflow);
        { Return(NumberConstant(0.0 - Smi::kMinValue)); }
      }
    }

    Bind(&if_xisnotsmi);
    {
      // Check if {x} is a HeapNumber.
      Label if_xisheapnumber(this), if_xisnotheapnumber(this, Label::kDeferred);
      Branch(IsHeapNumberMap(LoadMap(x)), &if_xisheapnumber,
             &if_xisnotheapnumber);

      Bind(&if_xisheapnumber);
      {
        Node* x_value = LoadHeapNumberValue(x);
        Node* value = Float64Abs(x_value);
        Node* result = AllocateHeapNumberWithValue(value);
        Return(result);
      }

      Bind(&if_xisnotheapnumber);
      {
        // Need to convert {x} to a Number first.
        Callable callable = CodeFactory::NonNumberToNumber(isolate());
        var_x.Bind(CallStub(callable, context, x));
        Goto(&loop);
      }
    }
  }
}

void MathBuiltinsAssembler::MathRoundingOperation(
    Node* (CodeStubAssembler::*float64op)(Node*)) {
  Node* context = Parameter(4);

  // We might need to loop once for ToNumber conversion.
  Variable var_x(this, MachineRepresentation::kTagged);
  Label loop(this, &var_x);
  var_x.Bind(Parameter(1));
  Goto(&loop);
  Bind(&loop);
  {
    // Load the current {x} value.
    Node* x = var_x.value();

    // Check if {x} is a Smi or a HeapObject.
    Label if_xissmi(this), if_xisnotsmi(this);
    Branch(TaggedIsSmi(x), &if_xissmi, &if_xisnotsmi);

    Bind(&if_xissmi);
    {
      // Nothing to do when {x} is a Smi.
      Return(x);
    }

    Bind(&if_xisnotsmi);
    {
      // Check if {x} is a HeapNumber.
      Label if_xisheapnumber(this), if_xisnotheapnumber(this, Label::kDeferred);
      Branch(IsHeapNumberMap(LoadMap(x)), &if_xisheapnumber,
             &if_xisnotheapnumber);

      Bind(&if_xisheapnumber);
      {
        Node* x_value = LoadHeapNumberValue(x);
        Node* value = (this->*float64op)(x_value);
        Node* result = ChangeFloat64ToTagged(value);
        Return(result);
      }

      Bind(&if_xisnotheapnumber);
      {
        // Need to convert {x} to a Number first.
        Callable callable = CodeFactory::NonNumberToNumber(isolate());
        var_x.Bind(CallStub(callable, context, x));
        Goto(&loop);
      }
    }
  }
}

void MathBuiltinsAssembler::MathUnaryOperation(
    Node* (CodeStubAssembler::*float64op)(Node*)) {
  Node* x = Parameter(1);
  Node* context = Parameter(4);
  Node* x_value = TruncateTaggedToFloat64(context, x);
  Node* value = (this->*float64op)(x_value);
  Node* result = AllocateHeapNumberWithValue(value);
  Return(result);
}

// ES6 section 20.2.2.2 Math.acos ( x )
TF_BUILTIN(MathAcos, MathBuiltinsAssembler) {
  MathUnaryOperation(&CodeStubAssembler::Float64Acos);
}

// ES6 section 20.2.2.3 Math.acosh ( x )
TF_BUILTIN(MathAcosh, MathBuiltinsAssembler) {
  MathUnaryOperation(&CodeStubAssembler::Float64Acosh);
}

// ES6 section 20.2.2.4 Math.asin ( x )
TF_BUILTIN(MathAsin, MathBuiltinsAssembler) {
  MathUnaryOperation(&CodeStubAssembler::Float64Asin);
}

// ES6 section 20.2.2.5 Math.asinh ( x )
TF_BUILTIN(MathAsinh, MathBuiltinsAssembler) {
  MathUnaryOperation(&CodeStubAssembler::Float64Asinh);
}
// ES6 section 20.2.2.6 Math.atan ( x )
TF_BUILTIN(MathAtan, MathBuiltinsAssembler) {
  MathUnaryOperation(&CodeStubAssembler::Float64Atan);
}

// ES6 section 20.2.2.7 Math.atanh ( x )
TF_BUILTIN(MathAtanh, MathBuiltinsAssembler) {
  MathUnaryOperation(&CodeStubAssembler::Float64Atanh);
}

// ES6 section 20.2.2.8 Math.atan2 ( y, x )
TF_BUILTIN(MathAtan2, CodeStubAssembler) {
  Node* y = Parameter(1);
  Node* x = Parameter(2);
  Node* context = Parameter(5);

  Node* y_value = TruncateTaggedToFloat64(context, y);
  Node* x_value = TruncateTaggedToFloat64(context, x);
  Node* value = Float64Atan2(y_value, x_value);
  Node* result = AllocateHeapNumberWithValue(value);
  Return(result);
}

// ES6 section 20.2.2.10 Math.ceil ( x )
TF_BUILTIN(MathCeil, MathBuiltinsAssembler) {
  MathRoundingOperation(&CodeStubAssembler::Float64Ceil);
}

// ES6 section 20.2.2.9 Math.cbrt ( x )
TF_BUILTIN(MathCbrt, MathBuiltinsAssembler) {
  MathUnaryOperation(&CodeStubAssembler::Float64Cbrt);
}

// ES6 section 20.2.2.11 Math.clz32 ( x )
TF_BUILTIN(MathClz32, CodeStubAssembler) {
  Node* context = Parameter(4);

  // Shared entry point for the clz32 operation.
  Variable var_clz32_x(this, MachineRepresentation::kWord32);
  Label do_clz32(this);

  // We might need to loop once for ToNumber conversion.
  Variable var_x(this, MachineRepresentation::kTagged);
  Label loop(this, &var_x);
  var_x.Bind(Parameter(1));
  Goto(&loop);
  Bind(&loop);
  {
    // Load the current {x} value.
    Node* x = var_x.value();

    // Check if {x} is a Smi or a HeapObject.
    Label if_xissmi(this), if_xisnotsmi(this);
    Branch(TaggedIsSmi(x), &if_xissmi, &if_xisnotsmi);

    Bind(&if_xissmi);
    {
      var_clz32_x.Bind(SmiToWord32(x));
      Goto(&do_clz32);
    }

    Bind(&if_xisnotsmi);
    {
      // Check if {x} is a HeapNumber.
      Label if_xisheapnumber(this), if_xisnotheapnumber(this, Label::kDeferred);
      Branch(IsHeapNumberMap(LoadMap(x)), &if_xisheapnumber,
             &if_xisnotheapnumber);

      Bind(&if_xisheapnumber);
      {
        var_clz32_x.Bind(TruncateHeapNumberValueToWord32(x));
        Goto(&do_clz32);
      }

      Bind(&if_xisnotheapnumber);
      {
        // Need to convert {x} to a Number first.
        Callable callable = CodeFactory::NonNumberToNumber(isolate());
        var_x.Bind(CallStub(callable, context, x));
        Goto(&loop);
      }
    }
  }

  Bind(&do_clz32);
  {
    Node* x_value = var_clz32_x.value();
    Node* value = Word32Clz(x_value);
    Node* result = ChangeInt32ToTagged(value);
    Return(result);
  }
}

// ES6 section 20.2.2.12 Math.cos ( x )
TF_BUILTIN(MathCos, MathBuiltinsAssembler) {
  MathUnaryOperation(&CodeStubAssembler::Float64Cos);
}

// ES6 section 20.2.2.13 Math.cosh ( x )
TF_BUILTIN(MathCosh, MathBuiltinsAssembler) {
  MathUnaryOperation(&CodeStubAssembler::Float64Cosh);
}

// ES6 section 20.2.2.14 Math.exp ( x )
TF_BUILTIN(MathExp, MathBuiltinsAssembler) {
  MathUnaryOperation(&CodeStubAssembler::Float64Exp);
}

// ES6 section 20.2.2.15 Math.expm1 ( x )
TF_BUILTIN(MathExpm1, MathBuiltinsAssembler) {
  MathUnaryOperation(&CodeStubAssembler::Float64Expm1);
}

// ES6 section 20.2.2.16 Math.floor ( x )
TF_BUILTIN(MathFloor, MathBuiltinsAssembler) {
  MathRoundingOperation(&CodeStubAssembler::Float64Floor);
}

// ES6 section 20.2.2.17 Math.fround ( x )
TF_BUILTIN(MathFround, CodeStubAssembler) {
  Node* x = Parameter(1);
  Node* context = Parameter(4);
  Node* x_value = TruncateTaggedToFloat64(context, x);
  Node* value32 = TruncateFloat64ToFloat32(x_value);
  Node* value = ChangeFloat32ToFloat64(value32);
  Node* result = AllocateHeapNumberWithValue(value);
  Return(result);
}

// ES6 section 20.2.2.18 Math.hypot ( value1, value2, ...values )
BUILTIN(MathHypot) {
  HandleScope scope(isolate);
  int const length = args.length() - 1;
  if (length == 0) return Smi::kZero;
  DCHECK_LT(0, length);
  double max = 0;
  bool one_arg_is_nan = false;
  List<double> abs_values(length);
  for (int i = 0; i < length; i++) {
    Handle<Object> x = args.at(i + 1);
    ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, x, Object::ToNumber(x));
    double abs_value = std::abs(x->Number());

    if (std::isnan(abs_value)) {
      one_arg_is_nan = true;
    } else {
      abs_values.Add(abs_value);
      if (max < abs_value) {
        max = abs_value;
      }
    }
  }

  if (max == V8_INFINITY) {
    return *isolate->factory()->NewNumber(V8_INFINITY);
  }

  if (one_arg_is_nan) {
    return isolate->heap()->nan_value();
  }

  if (max == 0) {
    return Smi::kZero;
  }
  DCHECK_GT(max, 0);

  // Kahan summation to avoid rounding errors.
  // Normalize the numbers to the largest one to avoid overflow.
  double sum = 0;
  double compensation = 0;
  for (int i = 0; i < length; i++) {
    double n = abs_values.at(i) / max;
    double summand = n * n - compensation;
    double preliminary = sum + summand;
    compensation = (preliminary - sum) - summand;
    sum = preliminary;
  }

  return *isolate->factory()->NewNumber(std::sqrt(sum) * max);
}

// ES6 section 20.2.2.19 Math.imul ( x, y )
TF_BUILTIN(MathImul, CodeStubAssembler) {
  Node* x = Parameter(1);
  Node* y = Parameter(2);
  Node* context = Parameter(5);
  Node* x_value = TruncateTaggedToWord32(context, x);
  Node* y_value = TruncateTaggedToWord32(context, y);
  Node* value = Int32Mul(x_value, y_value);
  Node* result = ChangeInt32ToTagged(value);
  Return(result);
}

// ES6 section 20.2.2.20 Math.log ( x )
TF_BUILTIN(MathLog, MathBuiltinsAssembler) {
  MathUnaryOperation(&CodeStubAssembler::Float64Log);
}

// ES6 section 20.2.2.21 Math.log1p ( x )
TF_BUILTIN(MathLog1p, MathBuiltinsAssembler) {
  MathUnaryOperation(&CodeStubAssembler::Float64Log1p);
}

// ES6 section 20.2.2.22 Math.log10 ( x )
TF_BUILTIN(MathLog10, MathBuiltinsAssembler) {
  MathUnaryOperation(&CodeStubAssembler::Float64Log10);
}

// ES6 section 20.2.2.23 Math.log2 ( x )
TF_BUILTIN(MathLog2, MathBuiltinsAssembler) {
  MathUnaryOperation(&CodeStubAssembler::Float64Log2);
}

// ES6 section 20.2.2.26 Math.pow ( x, y )
TF_BUILTIN(MathPow, CodeStubAssembler) {
  Node* x = Parameter(1);
  Node* y = Parameter(2);
  Node* context = Parameter(5);
  Node* x_value = TruncateTaggedToFloat64(context, x);
  Node* y_value = TruncateTaggedToFloat64(context, y);
  Node* value = Float64Pow(x_value, y_value);
  Node* result = ChangeFloat64ToTagged(value);
  Return(result);
}

// ES6 section 20.2.2.27 Math.random ( )
TF_BUILTIN(MathRandom, CodeStubAssembler) {
  Node* context = Parameter(3);
  Node* native_context = LoadNativeContext(context);

  // Load cache index.
  Variable smi_index(this, MachineRepresentation::kTagged);
  smi_index.Bind(
      LoadContextElement(native_context, Context::MATH_RANDOM_INDEX_INDEX));

  // Cached random numbers are exhausted if index is 0. Go to slow path.
  Label if_cached(this);
  GotoIf(SmiAbove(smi_index.value(), SmiConstant(Smi::kZero)), &if_cached);

  // Cache exhausted, populate the cache. Return value is the new index.
  smi_index.Bind(CallRuntime(Runtime::kGenerateRandomNumbers, context));
  Goto(&if_cached);

  // Compute next index by decrement.
  Bind(&if_cached);
  Node* new_smi_index = SmiSub(smi_index.value(), SmiConstant(Smi::FromInt(1)));
  StoreContextElement(native_context, Context::MATH_RANDOM_INDEX_INDEX,
                      new_smi_index);

  // Load and return next cached random number.
  Node* array =
      LoadContextElement(native_context, Context::MATH_RANDOM_CACHE_INDEX);
  Node* random = LoadFixedDoubleArrayElement(
      array, new_smi_index, MachineType::Float64(), 0, SMI_PARAMETERS);
  Return(AllocateHeapNumberWithValue(random));
}

// ES6 section 20.2.2.28 Math.round ( x )
TF_BUILTIN(MathRound, MathBuiltinsAssembler) {
  MathRoundingOperation(&CodeStubAssembler::Float64Round);
}

// ES6 section 20.2.2.29 Math.sign ( x )
TF_BUILTIN(MathSign, CodeStubAssembler) {
  // Convert the {x} value to a Number.
  Node* x = Parameter(1);
  Node* context = Parameter(4);
  Node* x_value = TruncateTaggedToFloat64(context, x);

  // Return -1 if {x} is negative, 1 if {x} is positive, or {x} itself.
  Label if_xisnegative(this), if_xispositive(this);
  GotoIf(Float64LessThan(x_value, Float64Constant(0.0)), &if_xisnegative);
  GotoIf(Float64LessThan(Float64Constant(0.0), x_value), &if_xispositive);
  Return(ChangeFloat64ToTagged(x_value));

  Bind(&if_xisnegative);
  Return(SmiConstant(Smi::FromInt(-1)));

  Bind(&if_xispositive);
  Return(SmiConstant(Smi::FromInt(1)));
}

// ES6 section 20.2.2.30 Math.sin ( x )
TF_BUILTIN(MathSin, MathBuiltinsAssembler) {
  MathUnaryOperation(&CodeStubAssembler::Float64Sin);
}

// ES6 section 20.2.2.31 Math.sinh ( x )
TF_BUILTIN(MathSinh, MathBuiltinsAssembler) {
  MathUnaryOperation(&CodeStubAssembler::Float64Sinh);
}

// ES6 section 20.2.2.32 Math.sqrt ( x )
TF_BUILTIN(MathSqrt, MathBuiltinsAssembler) {
  MathUnaryOperation(&CodeStubAssembler::Float64Sqrt);
}

// ES6 section 20.2.2.33 Math.tan ( x )
TF_BUILTIN(MathTan, MathBuiltinsAssembler) {
  MathUnaryOperation(&CodeStubAssembler::Float64Tan);
}

// ES6 section 20.2.2.34 Math.tanh ( x )
TF_BUILTIN(MathTanh, MathBuiltinsAssembler) {
  MathUnaryOperation(&CodeStubAssembler::Float64Tanh);
}

// ES6 section 20.2.2.35 Math.trunc ( x )
TF_BUILTIN(MathTrunc, MathBuiltinsAssembler) {
  MathRoundingOperation(&CodeStubAssembler::Float64Trunc);
}

void Builtins::Generate_MathMax(MacroAssembler* masm) {
  Generate_MathMaxMin(masm, MathMaxMinKind::kMax);
}

void Builtins::Generate_MathMin(MacroAssembler* masm) {
  Generate_MathMaxMin(masm, MathMaxMinKind::kMin);
}

}  // namespace internal
}  // namespace v8
