// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/builtins/builtins.h"
#include "src/builtins/builtins-utils.h"

#include "src/code-factory.h"

namespace v8 {
namespace internal {

// -----------------------------------------------------------------------------
// ES6 section 20.2.2 Function Properties of the Math Object

// ES6 section - 20.2.2.1 Math.abs ( x )
void Builtins::Generate_MathAbs(CodeStubAssembler* assembler) {
  using compiler::Node;
  Node* x = assembler->Parameter(1);
  Node* context = assembler->Parameter(4);
  Node* x_value = assembler->TruncateTaggedToFloat64(context, x);
  Node* value = assembler->Float64Abs(x_value);
  Node* result = assembler->ChangeFloat64ToTagged(value);
  assembler->Return(result);
}

// ES6 section 20.2.2.2 Math.acos ( x )
void Builtins::Generate_MathAcos(CodeStubAssembler* assembler) {
  using compiler::Node;

  Node* x = assembler->Parameter(1);
  Node* context = assembler->Parameter(4);
  Node* x_value = assembler->TruncateTaggedToFloat64(context, x);
  Node* value = assembler->Float64Acos(x_value);
  Node* result = assembler->ChangeFloat64ToTagged(value);
  assembler->Return(result);
}

// ES6 section 20.2.2.3 Math.acosh ( x )
void Builtins::Generate_MathAcosh(CodeStubAssembler* assembler) {
  using compiler::Node;

  Node* x = assembler->Parameter(1);
  Node* context = assembler->Parameter(4);
  Node* x_value = assembler->TruncateTaggedToFloat64(context, x);
  Node* value = assembler->Float64Acosh(x_value);
  Node* result = assembler->ChangeFloat64ToTagged(value);
  assembler->Return(result);
}

// ES6 section 20.2.2.4 Math.asin ( x )
void Builtins::Generate_MathAsin(CodeStubAssembler* assembler) {
  using compiler::Node;

  Node* x = assembler->Parameter(1);
  Node* context = assembler->Parameter(4);
  Node* x_value = assembler->TruncateTaggedToFloat64(context, x);
  Node* value = assembler->Float64Asin(x_value);
  Node* result = assembler->ChangeFloat64ToTagged(value);
  assembler->Return(result);
}

// ES6 section 20.2.2.5 Math.asinh ( x )
void Builtins::Generate_MathAsinh(CodeStubAssembler* assembler) {
  using compiler::Node;

  Node* x = assembler->Parameter(1);
  Node* context = assembler->Parameter(4);
  Node* x_value = assembler->TruncateTaggedToFloat64(context, x);
  Node* value = assembler->Float64Asinh(x_value);
  Node* result = assembler->ChangeFloat64ToTagged(value);
  assembler->Return(result);
}

// ES6 section 20.2.2.6 Math.atan ( x )
void Builtins::Generate_MathAtan(CodeStubAssembler* assembler) {
  using compiler::Node;

  Node* x = assembler->Parameter(1);
  Node* context = assembler->Parameter(4);
  Node* x_value = assembler->TruncateTaggedToFloat64(context, x);
  Node* value = assembler->Float64Atan(x_value);
  Node* result = assembler->ChangeFloat64ToTagged(value);
  assembler->Return(result);
}

// ES6 section 20.2.2.7 Math.atanh ( x )
void Builtins::Generate_MathAtanh(CodeStubAssembler* assembler) {
  using compiler::Node;

  Node* x = assembler->Parameter(1);
  Node* context = assembler->Parameter(4);
  Node* x_value = assembler->TruncateTaggedToFloat64(context, x);
  Node* value = assembler->Float64Atanh(x_value);
  Node* result = assembler->ChangeFloat64ToTagged(value);
  assembler->Return(result);
}

// ES6 section 20.2.2.8 Math.atan2 ( y, x )
void Builtins::Generate_MathAtan2(CodeStubAssembler* assembler) {
  using compiler::Node;

  Node* y = assembler->Parameter(1);
  Node* x = assembler->Parameter(2);
  Node* context = assembler->Parameter(5);
  Node* y_value = assembler->TruncateTaggedToFloat64(context, y);
  Node* x_value = assembler->TruncateTaggedToFloat64(context, x);
  Node* value = assembler->Float64Atan2(y_value, x_value);
  Node* result = assembler->ChangeFloat64ToTagged(value);
  assembler->Return(result);
}

namespace {

void Generate_MathRoundingOperation(
    CodeStubAssembler* assembler,
    compiler::Node* (CodeStubAssembler::*float64op)(compiler::Node*)) {
  typedef CodeStubAssembler::Label Label;
  typedef compiler::Node Node;
  typedef CodeStubAssembler::Variable Variable;

  Node* context = assembler->Parameter(4);

  // We might need to loop once for ToNumber conversion.
  Variable var_x(assembler, MachineRepresentation::kTagged);
  Label loop(assembler, &var_x);
  var_x.Bind(assembler->Parameter(1));
  assembler->Goto(&loop);
  assembler->Bind(&loop);
  {
    // Load the current {x} value.
    Node* x = var_x.value();

    // Check if {x} is a Smi or a HeapObject.
    Label if_xissmi(assembler), if_xisnotsmi(assembler);
    assembler->Branch(assembler->WordIsSmi(x), &if_xissmi, &if_xisnotsmi);

    assembler->Bind(&if_xissmi);
    {
      // Nothing to do when {x} is a Smi.
      assembler->Return(x);
    }

    assembler->Bind(&if_xisnotsmi);
    {
      // Check if {x} is a HeapNumber.
      Label if_xisheapnumber(assembler),
          if_xisnotheapnumber(assembler, Label::kDeferred);
      assembler->Branch(
          assembler->WordEqual(assembler->LoadMap(x),
                               assembler->HeapNumberMapConstant()),
          &if_xisheapnumber, &if_xisnotheapnumber);

      assembler->Bind(&if_xisheapnumber);
      {
        Node* x_value = assembler->LoadHeapNumberValue(x);
        Node* value = (assembler->*float64op)(x_value);
        Node* result = assembler->ChangeFloat64ToTagged(value);
        assembler->Return(result);
      }

      assembler->Bind(&if_xisnotheapnumber);
      {
        // Need to convert {x} to a Number first.
        Callable callable =
            CodeFactory::NonNumberToNumber(assembler->isolate());
        var_x.Bind(assembler->CallStub(callable, context, x));
        assembler->Goto(&loop);
      }
    }
  }
}

}  // namespace

// ES6 section 20.2.2.10 Math.ceil ( x )
void Builtins::Generate_MathCeil(CodeStubAssembler* assembler) {
  Generate_MathRoundingOperation(assembler, &CodeStubAssembler::Float64Ceil);
}

// ES6 section 20.2.2.9 Math.cbrt ( x )
void Builtins::Generate_MathCbrt(CodeStubAssembler* assembler) {
  using compiler::Node;

  Node* x = assembler->Parameter(1);
  Node* context = assembler->Parameter(4);
  Node* x_value = assembler->TruncateTaggedToFloat64(context, x);
  Node* value = assembler->Float64Cbrt(x_value);
  Node* result = assembler->ChangeFloat64ToTagged(value);
  assembler->Return(result);
}

// ES6 section 20.2.2.11 Math.clz32 ( x )
void Builtins::Generate_MathClz32(CodeStubAssembler* assembler) {
  typedef CodeStubAssembler::Label Label;
  typedef compiler::Node Node;
  typedef CodeStubAssembler::Variable Variable;

  Node* context = assembler->Parameter(4);

  // Shared entry point for the clz32 operation.
  Variable var_clz32_x(assembler, MachineRepresentation::kWord32);
  Label do_clz32(assembler);

  // We might need to loop once for ToNumber conversion.
  Variable var_x(assembler, MachineRepresentation::kTagged);
  Label loop(assembler, &var_x);
  var_x.Bind(assembler->Parameter(1));
  assembler->Goto(&loop);
  assembler->Bind(&loop);
  {
    // Load the current {x} value.
    Node* x = var_x.value();

    // Check if {x} is a Smi or a HeapObject.
    Label if_xissmi(assembler), if_xisnotsmi(assembler);
    assembler->Branch(assembler->WordIsSmi(x), &if_xissmi, &if_xisnotsmi);

    assembler->Bind(&if_xissmi);
    {
      var_clz32_x.Bind(assembler->SmiToWord32(x));
      assembler->Goto(&do_clz32);
    }

    assembler->Bind(&if_xisnotsmi);
    {
      // Check if {x} is a HeapNumber.
      Label if_xisheapnumber(assembler),
          if_xisnotheapnumber(assembler, Label::kDeferred);
      assembler->Branch(
          assembler->WordEqual(assembler->LoadMap(x),
                               assembler->HeapNumberMapConstant()),
          &if_xisheapnumber, &if_xisnotheapnumber);

      assembler->Bind(&if_xisheapnumber);
      {
        var_clz32_x.Bind(assembler->TruncateHeapNumberValueToWord32(x));
        assembler->Goto(&do_clz32);
      }

      assembler->Bind(&if_xisnotheapnumber);
      {
        // Need to convert {x} to a Number first.
        Callable callable =
            CodeFactory::NonNumberToNumber(assembler->isolate());
        var_x.Bind(assembler->CallStub(callable, context, x));
        assembler->Goto(&loop);
      }
    }
  }

  assembler->Bind(&do_clz32);
  {
    Node* x_value = var_clz32_x.value();
    Node* value = assembler->Word32Clz(x_value);
    Node* result = assembler->ChangeInt32ToTagged(value);
    assembler->Return(result);
  }
}

// ES6 section 20.2.2.12 Math.cos ( x )
void Builtins::Generate_MathCos(CodeStubAssembler* assembler) {
  using compiler::Node;

  Node* x = assembler->Parameter(1);
  Node* context = assembler->Parameter(4);
  Node* x_value = assembler->TruncateTaggedToFloat64(context, x);
  Node* value = assembler->Float64Cos(x_value);
  Node* result = assembler->ChangeFloat64ToTagged(value);
  assembler->Return(result);
}

// ES6 section 20.2.2.13 Math.cosh ( x )
void Builtins::Generate_MathCosh(CodeStubAssembler* assembler) {
  using compiler::Node;

  Node* x = assembler->Parameter(1);
  Node* context = assembler->Parameter(4);
  Node* x_value = assembler->TruncateTaggedToFloat64(context, x);
  Node* value = assembler->Float64Cosh(x_value);
  Node* result = assembler->ChangeFloat64ToTagged(value);
  assembler->Return(result);
}

// ES6 section 20.2.2.14 Math.exp ( x )
void Builtins::Generate_MathExp(CodeStubAssembler* assembler) {
  using compiler::Node;

  Node* x = assembler->Parameter(1);
  Node* context = assembler->Parameter(4);
  Node* x_value = assembler->TruncateTaggedToFloat64(context, x);
  Node* value = assembler->Float64Exp(x_value);
  Node* result = assembler->ChangeFloat64ToTagged(value);
  assembler->Return(result);
}

// ES6 section 20.2.2.16 Math.floor ( x )
void Builtins::Generate_MathFloor(CodeStubAssembler* assembler) {
  Generate_MathRoundingOperation(assembler, &CodeStubAssembler::Float64Floor);
}

// ES6 section 20.2.2.17 Math.fround ( x )
void Builtins::Generate_MathFround(CodeStubAssembler* assembler) {
  using compiler::Node;

  Node* x = assembler->Parameter(1);
  Node* context = assembler->Parameter(4);
  Node* x_value = assembler->TruncateTaggedToFloat64(context, x);
  Node* value32 = assembler->TruncateFloat64ToFloat32(x_value);
  Node* value = assembler->ChangeFloat32ToFloat64(value32);
  Node* result = assembler->ChangeFloat64ToTagged(value);
  assembler->Return(result);
}

// ES6 section 20.2.2.18 Math.hypot ( value1, value2, ...values )
BUILTIN(MathHypot) {
  HandleScope scope(isolate);
  int const length = args.length() - 1;
  if (length == 0) return Smi::FromInt(0);
  DCHECK_LT(0, length);
  double max = 0;
  bool one_arg_is_nan = false;
  List<double> abs_values(length);
  for (int i = 0; i < length; i++) {
    Handle<Object> x = args.at<Object>(i + 1);
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
    return *isolate->factory()->nan_value();
  }

  if (max == 0) {
    return Smi::FromInt(0);
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
void Builtins::Generate_MathImul(CodeStubAssembler* assembler) {
  using compiler::Node;

  Node* x = assembler->Parameter(1);
  Node* y = assembler->Parameter(2);
  Node* context = assembler->Parameter(5);
  Node* x_value = assembler->TruncateTaggedToWord32(context, x);
  Node* y_value = assembler->TruncateTaggedToWord32(context, y);
  Node* value = assembler->Int32Mul(x_value, y_value);
  Node* result = assembler->ChangeInt32ToTagged(value);
  assembler->Return(result);
}

// ES6 section 20.2.2.20 Math.log ( x )
void Builtins::Generate_MathLog(CodeStubAssembler* assembler) {
  using compiler::Node;

  Node* x = assembler->Parameter(1);
  Node* context = assembler->Parameter(4);
  Node* x_value = assembler->TruncateTaggedToFloat64(context, x);
  Node* value = assembler->Float64Log(x_value);
  Node* result = assembler->ChangeFloat64ToTagged(value);
  assembler->Return(result);
}

// ES6 section 20.2.2.21 Math.log1p ( x )
void Builtins::Generate_MathLog1p(CodeStubAssembler* assembler) {
  using compiler::Node;

  Node* x = assembler->Parameter(1);
  Node* context = assembler->Parameter(4);
  Node* x_value = assembler->TruncateTaggedToFloat64(context, x);
  Node* value = assembler->Float64Log1p(x_value);
  Node* result = assembler->ChangeFloat64ToTagged(value);
  assembler->Return(result);
}

// ES6 section 20.2.2.22 Math.log10 ( x )
void Builtins::Generate_MathLog10(CodeStubAssembler* assembler) {
  using compiler::Node;

  Node* x = assembler->Parameter(1);
  Node* context = assembler->Parameter(4);
  Node* x_value = assembler->TruncateTaggedToFloat64(context, x);
  Node* value = assembler->Float64Log10(x_value);
  Node* result = assembler->ChangeFloat64ToTagged(value);
  assembler->Return(result);
}

// ES6 section 20.2.2.23 Math.log2 ( x )
void Builtins::Generate_MathLog2(CodeStubAssembler* assembler) {
  using compiler::Node;

  Node* x = assembler->Parameter(1);
  Node* context = assembler->Parameter(4);
  Node* x_value = assembler->TruncateTaggedToFloat64(context, x);
  Node* value = assembler->Float64Log2(x_value);
  Node* result = assembler->ChangeFloat64ToTagged(value);
  assembler->Return(result);
}

// ES6 section 20.2.2.15 Math.expm1 ( x )
void Builtins::Generate_MathExpm1(CodeStubAssembler* assembler) {
  using compiler::Node;

  Node* x = assembler->Parameter(1);
  Node* context = assembler->Parameter(4);
  Node* x_value = assembler->TruncateTaggedToFloat64(context, x);
  Node* value = assembler->Float64Expm1(x_value);
  Node* result = assembler->ChangeFloat64ToTagged(value);
  assembler->Return(result);
}

// ES6 section 20.2.2.26 Math.pow ( x, y )
void Builtins::Generate_MathPow(CodeStubAssembler* assembler) {
  using compiler::Node;

  Node* x = assembler->Parameter(1);
  Node* y = assembler->Parameter(2);
  Node* context = assembler->Parameter(5);
  Node* x_value = assembler->TruncateTaggedToFloat64(context, x);
  Node* y_value = assembler->TruncateTaggedToFloat64(context, y);
  Node* value = assembler->Float64Pow(x_value, y_value);
  Node* result = assembler->ChangeFloat64ToTagged(value);
  assembler->Return(result);
}

// ES6 section 20.2.2.28 Math.round ( x )
void Builtins::Generate_MathRound(CodeStubAssembler* assembler) {
  Generate_MathRoundingOperation(assembler, &CodeStubAssembler::Float64Round);
}

// ES6 section 20.2.2.29 Math.sign ( x )
void Builtins::Generate_MathSign(CodeStubAssembler* assembler) {
  typedef CodeStubAssembler::Label Label;
  using compiler::Node;

  // Convert the {x} value to a Number.
  Node* x = assembler->Parameter(1);
  Node* context = assembler->Parameter(4);
  Node* x_value = assembler->TruncateTaggedToFloat64(context, x);

  // Return -1 if {x} is negative, 1 if {x} is positive, or {x} itself.
  Label if_xisnegative(assembler), if_xispositive(assembler);
  assembler->GotoIf(
      assembler->Float64LessThan(x_value, assembler->Float64Constant(0.0)),
      &if_xisnegative);
  assembler->GotoIf(
      assembler->Float64LessThan(assembler->Float64Constant(0.0), x_value),
      &if_xispositive);
  assembler->Return(assembler->ChangeFloat64ToTagged(x_value));

  assembler->Bind(&if_xisnegative);
  assembler->Return(assembler->SmiConstant(Smi::FromInt(-1)));

  assembler->Bind(&if_xispositive);
  assembler->Return(assembler->SmiConstant(Smi::FromInt(1)));
}

// ES6 section 20.2.2.30 Math.sin ( x )
void Builtins::Generate_MathSin(CodeStubAssembler* assembler) {
  using compiler::Node;

  Node* x = assembler->Parameter(1);
  Node* context = assembler->Parameter(4);
  Node* x_value = assembler->TruncateTaggedToFloat64(context, x);
  Node* value = assembler->Float64Sin(x_value);
  Node* result = assembler->ChangeFloat64ToTagged(value);
  assembler->Return(result);
}

// ES6 section 20.2.2.31 Math.sinh ( x )
void Builtins::Generate_MathSinh(CodeStubAssembler* assembler) {
  using compiler::Node;

  Node* x = assembler->Parameter(1);
  Node* context = assembler->Parameter(4);
  Node* x_value = assembler->TruncateTaggedToFloat64(context, x);
  Node* value = assembler->Float64Sinh(x_value);
  Node* result = assembler->ChangeFloat64ToTagged(value);
  assembler->Return(result);
}

// ES6 section 20.2.2.32 Math.sqrt ( x )
void Builtins::Generate_MathSqrt(CodeStubAssembler* assembler) {
  using compiler::Node;

  Node* x = assembler->Parameter(1);
  Node* context = assembler->Parameter(4);
  Node* x_value = assembler->TruncateTaggedToFloat64(context, x);
  Node* value = assembler->Float64Sqrt(x_value);
  Node* result = assembler->ChangeFloat64ToTagged(value);
  assembler->Return(result);
}

// ES6 section 20.2.2.33 Math.tan ( x )
void Builtins::Generate_MathTan(CodeStubAssembler* assembler) {
  using compiler::Node;

  Node* x = assembler->Parameter(1);
  Node* context = assembler->Parameter(4);
  Node* x_value = assembler->TruncateTaggedToFloat64(context, x);
  Node* value = assembler->Float64Tan(x_value);
  Node* result = assembler->ChangeFloat64ToTagged(value);
  assembler->Return(result);
}

// ES6 section 20.2.2.34 Math.tanh ( x )
void Builtins::Generate_MathTanh(CodeStubAssembler* assembler) {
  using compiler::Node;

  Node* x = assembler->Parameter(1);
  Node* context = assembler->Parameter(4);
  Node* x_value = assembler->TruncateTaggedToFloat64(context, x);
  Node* value = assembler->Float64Tanh(x_value);
  Node* result = assembler->ChangeFloat64ToTagged(value);
  assembler->Return(result);
}

// ES6 section 20.2.2.35 Math.trunc ( x )
void Builtins::Generate_MathTrunc(CodeStubAssembler* assembler) {
  Generate_MathRoundingOperation(assembler, &CodeStubAssembler::Float64Trunc);
}

void Builtins::Generate_MathMax(MacroAssembler* masm) {
  Generate_MathMaxMin(masm, MathMaxMinKind::kMax);
}

void Builtins::Generate_MathMin(MacroAssembler* masm) {
  Generate_MathMaxMin(masm, MathMaxMinKind::kMin);
}

}  // namespace internal
}  // namespace v8
