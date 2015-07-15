// Copyright 2006-2008 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This files contains runtime support implemented in JavaScript.

// CAUTION: Some of the functions specified in this file are called
// directly from compiled code. These are the functions with names in
// ALL CAPS. The compiled code passes the first argument in 'this'.


/* -----------------------------------
   - - -   C o m p a r i s o n   - - -
   -----------------------------------
*/

// The following declarations are shared with other native JS files.
// They are all declared at this one spot to avoid redeclaration errors.
var EQUALS;
var STRICT_EQUALS;
var COMPARE;
var COMPARE_STRONG;
var ADD;
var ADD_STRONG;
var STRING_ADD_LEFT;
var STRING_ADD_LEFT_STRONG;
var STRING_ADD_RIGHT;
var STRING_ADD_RIGHT_STRONG;
var SUB;
var SUB_STRONG;
var MUL;
var MUL_STRONG;
var DIV;
var DIV_STRONG;
var MOD;
var MOD_STRONG;
var BIT_OR;
var BIT_OR_STRONG;
var BIT_AND;
var BIT_AND_STRONG;
var BIT_XOR;
var BIT_XOR_STRONG;
var SHL;
var SHL_STRONG;
var SAR;
var SAR_STRONG;
var SHR;
var SHR_STRONG;
var DELETE;
var IN;
var INSTANCE_OF;
var CALL_NON_FUNCTION;
var CALL_NON_FUNCTION_AS_CONSTRUCTOR;
var CALL_FUNCTION_PROXY;
var CALL_FUNCTION_PROXY_AS_CONSTRUCTOR;
var CONCAT_ITERABLE_TO_ARRAY;
var APPLY_PREPARE;
var REFLECT_APPLY_PREPARE;
var REFLECT_CONSTRUCT_PREPARE;
var STACK_OVERFLOW;
var TO_OBJECT;
var TO_NUMBER;
var TO_STRING;
var TO_NAME;

var StringLengthTFStub;
var StringAddTFStub;
var MathFloorStub;

var $defaultNumber;
var $defaultString;
var $NaN;
var $nonNumberToNumber;
var $nonStringToString;
var $sameValue;
var $sameValueZero;
var $toBoolean;
var $toInt32;
var $toInteger;
var $toLength;
var $toName;
var $toNumber;
var $toObject;
var $toPositiveInteger;
var $toPrimitive;
var $toString;
var $toUint32;

(function(global, utils) {

%CheckIsBootstrapping();

var GlobalArray = global.Array;
var GlobalBoolean = global.Boolean;
var GlobalString = global.String;
var GlobalNumber = global.Number;

// ----------------------------------------------------------------------------

// ECMA-262 Section 11.9.3.
EQUALS = function EQUALS(y) {
  if (IS_STRING(this) && IS_STRING(y)) return %StringEquals(this, y);
  var x = this;

  while (true) {
    if (IS_NUMBER(x)) {
      while (true) {
        if (IS_NUMBER(y)) return %NumberEquals(x, y);
        if (IS_NULL_OR_UNDEFINED(y)) return 1;  // not equal
        if (IS_SYMBOL(y)) return 1;  // not equal
        if (!IS_SPEC_OBJECT(y)) {
          // String or boolean.
          return %NumberEquals(x, %$toNumber(y));
        }
        y = %$toPrimitive(y, NO_HINT);
      }
    } else if (IS_STRING(x)) {
      while (true) {
        if (IS_STRING(y)) return %StringEquals(x, y);
        if (IS_SYMBOL(y)) return 1;  // not equal
        if (IS_NUMBER(y)) return %NumberEquals(%$toNumber(x), y);
        if (IS_BOOLEAN(y)) return %NumberEquals(%$toNumber(x), %$toNumber(y));
        if (IS_NULL_OR_UNDEFINED(y)) return 1;  // not equal
        y = %$toPrimitive(y, NO_HINT);
      }
    } else if (IS_SYMBOL(x)) {
      if (IS_SYMBOL(y)) return %_ObjectEquals(x, y) ? 0 : 1;
      return 1; // not equal
    } else if (IS_BOOLEAN(x)) {
      if (IS_BOOLEAN(y)) return %_ObjectEquals(x, y) ? 0 : 1;
      if (IS_NULL_OR_UNDEFINED(y)) return 1;
      if (IS_NUMBER(y)) return %NumberEquals(%$toNumber(x), y);
      if (IS_STRING(y)) return %NumberEquals(%$toNumber(x), %$toNumber(y));
      if (IS_SYMBOL(y)) return 1;  // not equal
      // y is object.
      x = %$toNumber(x);
      y = %$toPrimitive(y, NO_HINT);
    } else if (IS_NULL_OR_UNDEFINED(x)) {
      return IS_NULL_OR_UNDEFINED(y) ? 0 : 1;
    } else {
      // x is an object.
      if (IS_SPEC_OBJECT(y)) {
        return %_ObjectEquals(x, y) ? 0 : 1;
      }
      if (IS_NULL_OR_UNDEFINED(y)) return 1;  // not equal
      if (IS_SYMBOL(y)) return 1;  // not equal
      if (IS_BOOLEAN(y)) y = %$toNumber(y);
      x = %$toPrimitive(x, NO_HINT);
    }
  }
}

// ECMA-262, section 11.9.4, page 56.
STRICT_EQUALS = function STRICT_EQUALS(x) {
  if (IS_STRING(this)) {
    if (!IS_STRING(x)) return 1;  // not equal
    return %StringEquals(this, x);
  }

  if (IS_NUMBER(this)) {
    if (!IS_NUMBER(x)) return 1;  // not equal
    return %NumberEquals(this, x);
  }

  // If anything else gets here, we just do simple identity check.
  // Objects (including functions), null, undefined and booleans were
  // checked in the CompareStub, so there should be nothing left.
  return %_ObjectEquals(this, x) ? 0 : 1;
}


// ECMA-262, section 11.8.5, page 53. The 'ncr' parameter is used as
// the result when either (or both) the operands are NaN.
COMPARE = function COMPARE(x, ncr) {
  var left;
  var right;
  // Fast cases for string, numbers and undefined compares.
  if (IS_STRING(this)) {
    if (IS_STRING(x)) return %_StringCompare(this, x);
    if (IS_UNDEFINED(x)) return ncr;
    left = this;
  } else if (IS_NUMBER(this)) {
    if (IS_NUMBER(x)) return %NumberCompare(this, x, ncr);
    if (IS_UNDEFINED(x)) return ncr;
    left = this;
  } else if (IS_UNDEFINED(this)) {
    if (!IS_UNDEFINED(x)) {
      %$toPrimitive(x, NUMBER_HINT);
    }
    return ncr;
  } else if (IS_UNDEFINED(x)) {
    %$toPrimitive(this, NUMBER_HINT);
    return ncr;
  } else {
    left = %$toPrimitive(this, NUMBER_HINT);
  }

  right = %$toPrimitive(x, NUMBER_HINT);
  if (IS_STRING(left) && IS_STRING(right)) {
    return %_StringCompare(left, right);
  } else {
    var left_number = %$toNumber(left);
    var right_number = %$toNumber(right);
    if (NUMBER_IS_NAN(left_number) || NUMBER_IS_NAN(right_number)) return ncr;
    return %NumberCompare(left_number, right_number, ncr);
  }
}

// Strong mode COMPARE throws if an implicit conversion would be performed
COMPARE_STRONG = function COMPARE_STRONG(x, ncr) {
  if (IS_STRING(this) && IS_STRING(x)) return %_StringCompare(this, x);
  if (IS_NUMBER(this) && IS_NUMBER(x)) return %NumberCompare(this, x, ncr);

  throw %MakeTypeError(kStrongImplicitConversion);
}



/* -----------------------------------
   - - -   A r i t h m e t i c   - - -
   -----------------------------------
*/

// ECMA-262, section 11.6.1, page 50.
ADD = function ADD(x) {
  // Fast case: Check for number operands and do the addition.
  if (IS_NUMBER(this) && IS_NUMBER(x)) return %NumberAdd(this, x);
  if (IS_STRING(this) && IS_STRING(x)) return %_StringAdd(this, x);

  // Default implementation.
  var a = %$toPrimitive(this, NO_HINT);
  var b = %$toPrimitive(x, NO_HINT);

  if (IS_STRING(a)) {
    return %_StringAdd(a, %$toString(b));
  } else if (IS_STRING(b)) {
    return %_StringAdd(%$nonStringToString(a), b);
  } else {
    return %NumberAdd(%$toNumber(a), %$toNumber(b));
  }
}


// Strong mode ADD throws if an implicit conversion would be performed
ADD_STRONG = function ADD_STRONG(x) {
  if (IS_NUMBER(this) && IS_NUMBER(x)) return %NumberAdd(this, x);
  if (IS_STRING(this) && IS_STRING(x)) return %_StringAdd(this, x);

  throw %MakeTypeError(kStrongImplicitConversion);
}


// Left operand (this) is already a string.
STRING_ADD_LEFT = function STRING_ADD_LEFT(y) {
  if (!IS_STRING(y)) {
    if (IS_STRING_WRAPPER(y) && %_IsStringWrapperSafeForDefaultValueOf(y)) {
      y = %_ValueOf(y);
    } else {
      y = IS_NUMBER(y)
          ? %_NumberToString(y)
          : %$toString(%$toPrimitive(y, NO_HINT));
    }
  }
  return %_StringAdd(this, y);
}


// Left operand (this) is already a string.
STRING_ADD_LEFT_STRONG = function STRING_ADD_LEFT_STRONG(y) {
  if (IS_STRING(y)) {
    return %_StringAdd(this, y);
  }
  throw %MakeTypeError(kStrongImplicitConversion);
}


// Right operand (y) is already a string.
STRING_ADD_RIGHT = function STRING_ADD_RIGHT(y) {
  var x = this;
  if (!IS_STRING(x)) {
    if (IS_STRING_WRAPPER(x) && %_IsStringWrapperSafeForDefaultValueOf(x)) {
      x = %_ValueOf(x);
    } else {
      x = IS_NUMBER(x)
          ? %_NumberToString(x)
          : %$toString(%$toPrimitive(x, NO_HINT));
    }
  }
  return %_StringAdd(x, y);
}


// Right operand (y) is already a string.
STRING_ADD_RIGHT_STRONG = function STRING_ADD_RIGHT_STRONG(y) {
  if (IS_STRING(this)) {
    return %_StringAdd(this, y);
  }
  throw %MakeTypeError(kStrongImplicitConversion);
}


// ECMA-262, section 11.6.2, page 50.
SUB = function SUB(y) {
  var x = IS_NUMBER(this) ? this : %$nonNumberToNumber(this);
  if (!IS_NUMBER(y)) y = %$nonNumberToNumber(y);
  return %NumberSub(x, y);
}


// Strong mode SUB throws if an implicit conversion would be performed
SUB_STRONG = function SUB_STRONG(y) {
  if (IS_NUMBER(this) && IS_NUMBER(y)) {
    return %NumberSub(this, y);
  }
  throw %MakeTypeError(kStrongImplicitConversion);
}


// ECMA-262, section 11.5.1, page 48.
MUL = function MUL(y) {
  var x = IS_NUMBER(this) ? this : %$nonNumberToNumber(this);
  if (!IS_NUMBER(y)) y = %$nonNumberToNumber(y);
  return %NumberMul(x, y);
}


// Strong mode MUL throws if an implicit conversion would be performed
MUL_STRONG = function MUL_STRONG(y) {
  if (IS_NUMBER(this) && IS_NUMBER(y)) {
    return %NumberMul(this, y);
  }
  throw %MakeTypeError(kStrongImplicitConversion);
}


// ECMA-262, section 11.5.2, page 49.
DIV = function DIV(y) {
  var x = IS_NUMBER(this) ? this : %$nonNumberToNumber(this);
  if (!IS_NUMBER(y)) y = %$nonNumberToNumber(y);
  return %NumberDiv(x, y);
}


// Strong mode DIV throws if an implicit conversion would be performed
DIV_STRONG = function DIV_STRONG(y) {
  if (IS_NUMBER(this) && IS_NUMBER(y)) {
    return %NumberDiv(this, y);
  }
  throw %MakeTypeError(kStrongImplicitConversion);
}


// ECMA-262, section 11.5.3, page 49.
MOD = function MOD(y) {
  var x = IS_NUMBER(this) ? this : %$nonNumberToNumber(this);
  if (!IS_NUMBER(y)) y = %$nonNumberToNumber(y);
  return %NumberMod(x, y);
}


// Strong mode MOD throws if an implicit conversion would be performed
MOD_STRONG = function MOD_STRONG(y) {
  if (IS_NUMBER(this) && IS_NUMBER(y)) {
    return %NumberMod(this, y);
  }
  throw %MakeTypeError(kStrongImplicitConversion);
}


/* -------------------------------------------
   - - -   B i t   o p e r a t i o n s   - - -
   -------------------------------------------
*/

// ECMA-262, section 11.10, page 57.
BIT_OR = function BIT_OR(y) {
  var x = IS_NUMBER(this) ? this : %$nonNumberToNumber(this);
  if (!IS_NUMBER(y)) y = %$nonNumberToNumber(y);
  return %NumberOr(x, y);
}


// Strong mode BIT_OR throws if an implicit conversion would be performed
BIT_OR_STRONG = function BIT_OR_STRONG(y) {
  if (IS_NUMBER(this) && IS_NUMBER(y)) {
    return %NumberOr(this, y);
  }
  throw %MakeTypeError(kStrongImplicitConversion);
}


// ECMA-262, section 11.10, page 57.
BIT_AND = function BIT_AND(y) {
  var x;
  if (IS_NUMBER(this)) {
    x = this;
    if (!IS_NUMBER(y)) y = %$nonNumberToNumber(y);
  } else {
    x = %$nonNumberToNumber(this);
    // Make sure to convert the right operand to a number before
    // bailing out in the fast case, but after converting the
    // left operand. This ensures that valueOf methods on the right
    // operand are always executed.
    if (!IS_NUMBER(y)) y = %$nonNumberToNumber(y);
    // Optimize for the case where we end up AND'ing a value
    // that doesn't convert to a number. This is common in
    // certain benchmarks.
    if (NUMBER_IS_NAN(x)) return 0;
  }
  return %NumberAnd(x, y);
}


// Strong mode BIT_AND throws if an implicit conversion would be performed
BIT_AND_STRONG = function BIT_AND_STRONG(y) {
  if (IS_NUMBER(this) && IS_NUMBER(y)) {
    return %NumberAnd(this, y);
  }
  throw %MakeTypeError(kStrongImplicitConversion);
}


// ECMA-262, section 11.10, page 57.
BIT_XOR = function BIT_XOR(y) {
  var x = IS_NUMBER(this) ? this : %$nonNumberToNumber(this);
  if (!IS_NUMBER(y)) y = %$nonNumberToNumber(y);
  return %NumberXor(x, y);
}


// Strong mode BIT_XOR throws if an implicit conversion would be performed
BIT_XOR_STRONG = function BIT_XOR_STRONG(y) {
  if (IS_NUMBER(this) && IS_NUMBER(y)) {
    return %NumberXor(this, y);
  }
  throw %MakeTypeError(kStrongImplicitConversion);
}


// ECMA-262, section 11.7.1, page 51.
SHL = function SHL(y) {
  var x = IS_NUMBER(this) ? this : %$nonNumberToNumber(this);
  if (!IS_NUMBER(y)) y = %$nonNumberToNumber(y);
  return %NumberShl(x, y);
}


// Strong mode SHL throws if an implicit conversion would be performed
SHL_STRONG = function SHL_STRONG(y) {
  if (IS_NUMBER(this) && IS_NUMBER(y)) {
    return %NumberShl(this, y);
  }
  throw %MakeTypeError(kStrongImplicitConversion);
}


// ECMA-262, section 11.7.2, page 51.
SAR = function SAR(y) {
  var x;
  if (IS_NUMBER(this)) {
    x = this;
    if (!IS_NUMBER(y)) y = %$nonNumberToNumber(y);
  } else {
    x = %$nonNumberToNumber(this);
    // Make sure to convert the right operand to a number before
    // bailing out in the fast case, but after converting the
    // left operand. This ensures that valueOf methods on the right
    // operand are always executed.
    if (!IS_NUMBER(y)) y = %$nonNumberToNumber(y);
    // Optimize for the case where we end up shifting a value
    // that doesn't convert to a number. This is common in
    // certain benchmarks.
    if (NUMBER_IS_NAN(x)) return 0;
  }
  return %NumberSar(x, y);
}


// Strong mode SAR throws if an implicit conversion would be performed
SAR_STRONG = function SAR_STRONG(y) {
  if (IS_NUMBER(this) && IS_NUMBER(y)) {
    return %NumberSar(this, y);
  }
  throw %MakeTypeError(kStrongImplicitConversion);
}


// ECMA-262, section 11.7.3, page 52.
SHR = function SHR(y) {
  var x = IS_NUMBER(this) ? this : %$nonNumberToNumber(this);
  if (!IS_NUMBER(y)) y = %$nonNumberToNumber(y);
  return %NumberShr(x, y);
}


// Strong mode SHR throws if an implicit conversion would be performed
SHR_STRONG = function SHR_STRONG(y) {
  if (IS_NUMBER(this) && IS_NUMBER(y)) {
    return %NumberShr(this, y);
  }
  throw %MakeTypeError(kStrongImplicitConversion);
}


/* -----------------------------
   - - -   H e l p e r s   - - -
   -----------------------------
*/

// ECMA-262, section 11.4.1, page 46.
DELETE = function DELETE(key, language_mode) {
  return %DeleteProperty(%$toObject(this), key, language_mode);
}


// ECMA-262, section 11.8.7, page 54.
IN = function IN(x) {
  if (!IS_SPEC_OBJECT(x)) {
    throw %MakeTypeError(kInvalidInOperatorUse, this, x);
  }
  if (%_IsNonNegativeSmi(this)) {
    if (IS_ARRAY(x) && %_HasFastPackedElements(x)) {
      return this < x.length;
    }
    return %HasElement(x, this);
  }
  return %HasProperty(x, %$toName(this));
}


// ECMA-262, section 11.8.6, page 54. To make the implementation more
// efficient, the return value should be zero if the 'this' is an
// instance of F, and non-zero if not. This makes it possible to avoid
// an expensive ToBoolean conversion in the generated code.
INSTANCE_OF = function INSTANCE_OF(F) {
  var V = this;
  if (!IS_SPEC_FUNCTION(F)) {
    throw %MakeTypeError(kInstanceofFunctionExpected, F);
  }

  // If V is not an object, return false.
  if (!IS_SPEC_OBJECT(V)) {
    return 1;
  }

  // Check if function is bound, if so, get [[BoundFunction]] from it
  // and use that instead of F.
  var bindings = %BoundFunctionGetBindings(F);
  if (bindings) {
    F = bindings[kBoundFunctionIndex];  // Always a non-bound function.
  }
  // Get the prototype of F; if it is not an object, throw an error.
  var O = F.prototype;
  if (!IS_SPEC_OBJECT(O)) {
    throw %MakeTypeError(kInstanceofNonobjectProto, O);
  }

  // Return whether or not O is in the prototype chain of V.
  return %IsInPrototypeChain(O, V) ? 0 : 1;
}


CALL_NON_FUNCTION = function CALL_NON_FUNCTION() {
  var delegate = %GetFunctionDelegate(this);
  if (!IS_FUNCTION(delegate)) {
    var callsite = %RenderCallSite();
    if (callsite == "") callsite = typeof this;
    throw %MakeTypeError(kCalledNonCallable, callsite);
  }
  return %Apply(delegate, this, arguments, 0, %_ArgumentsLength());
}


CALL_NON_FUNCTION_AS_CONSTRUCTOR = function CALL_NON_FUNCTION_AS_CONSTRUCTOR() {
  var delegate = %GetConstructorDelegate(this);
  if (!IS_FUNCTION(delegate)) {
    var callsite = %RenderCallSite();
    if (callsite == "") callsite = typeof this;
    throw %MakeTypeError(kCalledNonCallable, callsite);
  }
  return %Apply(delegate, this, arguments, 0, %_ArgumentsLength());
}


CALL_FUNCTION_PROXY = function CALL_FUNCTION_PROXY() {
  var arity = %_ArgumentsLength() - 1;
  var proxy = %_Arguments(arity);  // The proxy comes in as an additional arg.
  var trap = %GetCallTrap(proxy);
  return %Apply(trap, this, arguments, 0, arity);
}


CALL_FUNCTION_PROXY_AS_CONSTRUCTOR =
    function CALL_FUNCTION_PROXY_AS_CONSTRUCTOR () {
  var proxy = this;
  var trap = %GetConstructTrap(proxy);
  return %Apply(trap, this, arguments, 0, %_ArgumentsLength());
}


APPLY_PREPARE = function APPLY_PREPARE(args) {
  var length;
  // First check whether length is a positive Smi and args is an
  // array. This is the fast case. If this fails, we do the slow case
  // that takes care of more eventualities.
  if (IS_ARRAY(args)) {
    length = args.length;
    if (%_IsSmi(length) && length >= 0 && length < kSafeArgumentsLength &&
        IS_SPEC_FUNCTION(this)) {
      return length;
    }
  }

  length = (args == null) ? 0 : %$toUint32(args.length);

  // We can handle any number of apply arguments if the stack is
  // big enough, but sanity check the value to avoid overflow when
  // multiplying with pointer size.
  if (length > kSafeArgumentsLength) throw %MakeRangeError(kStackOverflow);

  if (!IS_SPEC_FUNCTION(this)) {
    throw %MakeTypeError(kApplyNonFunction, %$toString(this), typeof this);
  }

  // Make sure the arguments list has the right type.
  if (args != null && !IS_SPEC_OBJECT(args)) {
    throw %MakeTypeError(kWrongArgs, "Function.prototype.apply");
  }

  // Return the length which is the number of arguments to copy to the
  // stack. It is guaranteed to be a small integer at this point.
  return length;
}


REFLECT_APPLY_PREPARE = function REFLECT_APPLY_PREPARE(args) {
  var length;
  // First check whether length is a positive Smi and args is an
  // array. This is the fast case. If this fails, we do the slow case
  // that takes care of more eventualities.
  if (IS_ARRAY(args)) {
    length = args.length;
    if (%_IsSmi(length) && length >= 0 && length < kSafeArgumentsLength &&
        IS_SPEC_FUNCTION(this)) {
      return length;
    }
  }

  if (!IS_SPEC_FUNCTION(this)) {
    throw %MakeTypeError(kCalledNonCallable, %$toString(this));
  }

  if (!IS_SPEC_OBJECT(args)) {
    throw %MakeTypeError(kWrongArgs, "Reflect.apply");
  }

  length = %$toLength(args.length);

  // We can handle any number of apply arguments if the stack is
  // big enough, but sanity check the value to avoid overflow when
  // multiplying with pointer size.
  if (length > kSafeArgumentsLength) throw %MakeRangeError(kStackOverflow);

  // Return the length which is the number of arguments to copy to the
  // stack. It is guaranteed to be a small integer at this point.
  return length;
}


REFLECT_CONSTRUCT_PREPARE = function REFLECT_CONSTRUCT_PREPARE(
    args, newTarget) {
  var length;
  var ctorOk = IS_SPEC_FUNCTION(this) && %IsConstructor(this);
  var newTargetOk = IS_SPEC_FUNCTION(newTarget) && %IsConstructor(newTarget);

  // First check whether length is a positive Smi and args is an
  // array. This is the fast case. If this fails, we do the slow case
  // that takes care of more eventualities.
  if (IS_ARRAY(args)) {
    length = args.length;
    if (%_IsSmi(length) && length >= 0 && length < kSafeArgumentsLength &&
        ctorOk && newTargetOk) {
      return length;
    }
  }

  if (!ctorOk) {
    if (!IS_SPEC_FUNCTION(this)) {
      throw %MakeTypeError(kCalledNonCallable, %$toString(this));
    } else {
      throw %MakeTypeError(kNotConstructor, %$toString(this));
    }
  }

  if (!newTargetOk) {
    if (!IS_SPEC_FUNCTION(newTarget)) {
      throw %MakeTypeError(kCalledNonCallable, %$toString(newTarget));
    } else {
      throw %MakeTypeError(kNotConstructor, %$toString(newTarget));
    }
  }

  if (!IS_SPEC_OBJECT(args)) {
    throw %MakeTypeError(kWrongArgs, "Reflect.construct");
  }

  length = %$toLength(args.length);

  // We can handle any number of apply arguments if the stack is
  // big enough, but sanity check the value to avoid overflow when
  // multiplying with pointer size.
  if (length > kSafeArgumentsLength) throw %MakeRangeError(kStackOverflow);

  // Return the length which is the number of arguments to copy to the
  // stack. It is guaranteed to be a small integer at this point.
  return length;
}


CONCAT_ITERABLE_TO_ARRAY = function CONCAT_ITERABLE_TO_ARRAY(iterable) {
  return %$concatIterableToArray(this, iterable);
};


STACK_OVERFLOW = function STACK_OVERFLOW(length) {
  throw %MakeRangeError(kStackOverflow);
}


// Convert the receiver to an object - forward to ToObject.
TO_OBJECT = function TO_OBJECT() {
  return %$toObject(this);
}


// Convert the receiver to a number - forward to ToNumber.
TO_NUMBER = function TO_NUMBER() {
  return %$toNumber(this);
}


// Convert the receiver to a string - forward to ToString.
TO_STRING = function TO_STRING() {
  return %$toString(this);
}


// Convert the receiver to a string or symbol - forward to ToName.
TO_NAME = function TO_NAME() {
  return %$toName(this);
}


/* -----------------------------------------------
   - - -   J a v a S c r i p t   S t u b s   - - -
   -----------------------------------------------
*/

StringLengthTFStub = function StringLengthTFStub(call_conv, minor_key) {
  var stub = function(receiver, name, i, v) {
    // i and v are dummy parameters mandated by the InterfaceDescriptor,
    // (LoadWithVectorDescriptor).
    return %_StringGetLength(%_JSValueGetValue(receiver));
  }
  return stub;
}

StringAddTFStub = function StringAddTFStub(call_conv, minor_key) {
  var stub = function(left, right) {
    return %StringAdd(left, right);
  }
  return stub;
}

MathFloorStub = function MathFloorStub(call_conv, minor_key) {
  var stub = function(f, i, v) {
    // |f| is calling function's JSFunction
    // |i| is TypeFeedbackVector slot # of callee's CallIC for Math.floor call
    // |v| is the value to floor
    var r = %_MathFloor(+v);
    if (%_IsMinusZero(r)) {
      // Collect type feedback when the result of the floor is -0. This is
      // accomplished by storing a sentinel in the second, "extra"
      // TypeFeedbackVector slot corresponding to the Math.floor CallIC call in
      // the caller's TypeVector.
      %_FixedArraySet(%_GetTypeFeedbackVector(f), ((i|0)+1)|0, 1);
      return -0;
    }
    // Return integers in smi range as smis.
    var trunc = r|0;
    if (trunc === r) {
      return trunc;
    }
    return r;
  }
  return stub;
}


/* -------------------------------------
   - - -   C o n v e r s i o n s   - - -
   -------------------------------------
*/

// ECMA-262, section 9.1, page 30. Use null/undefined for no hint,
// (1) for number hint, and (2) for string hint.
function ToPrimitive(x, hint) {
  // Fast case check.
  if (IS_STRING(x)) return x;
  // Normal behavior.
  if (!IS_SPEC_OBJECT(x)) return x;
  if (IS_SYMBOL_WRAPPER(x)) throw MakeTypeError(kSymbolToPrimitive);
  if (hint == NO_HINT) hint = (IS_DATE(x)) ? STRING_HINT : NUMBER_HINT;
  return (hint == NUMBER_HINT) ? DefaultNumber(x) : DefaultString(x);
}


// ECMA-262, section 9.2, page 30
function ToBoolean(x) {
  if (IS_BOOLEAN(x)) return x;
  if (IS_STRING(x)) return x.length != 0;
  if (x == null) return false;
  if (IS_NUMBER(x)) return !((x == 0) || NUMBER_IS_NAN(x));
  return true;
}


// ECMA-262, section 9.3, page 31.
function ToNumber(x) {
  if (IS_NUMBER(x)) return x;
  if (IS_STRING(x)) {
    return %_HasCachedArrayIndex(x) ? %_GetCachedArrayIndex(x)
                                    : %StringToNumber(x);
  }
  if (IS_BOOLEAN(x)) return x ? 1 : 0;
  if (IS_UNDEFINED(x)) return NAN;
  if (IS_SYMBOL(x)) throw MakeTypeError(kSymbolToNumber);
  return (IS_NULL(x)) ? 0 : ToNumber(DefaultNumber(x));
}

function NonNumberToNumber(x) {
  if (IS_STRING(x)) {
    return %_HasCachedArrayIndex(x) ? %_GetCachedArrayIndex(x)
                                    : %StringToNumber(x);
  }
  if (IS_BOOLEAN(x)) return x ? 1 : 0;
  if (IS_UNDEFINED(x)) return NAN;
  if (IS_SYMBOL(x)) throw MakeTypeError(kSymbolToNumber);
  return (IS_NULL(x)) ? 0 : ToNumber(DefaultNumber(x));
}


// ECMA-262, section 9.8, page 35.
function ToString(x) {
  if (IS_STRING(x)) return x;
  if (IS_NUMBER(x)) return %_NumberToString(x);
  if (IS_BOOLEAN(x)) return x ? 'true' : 'false';
  if (IS_UNDEFINED(x)) return 'undefined';
  if (IS_SYMBOL(x)) throw MakeTypeError(kSymbolToString);
  return (IS_NULL(x)) ? 'null' : ToString(DefaultString(x));
}

function NonStringToString(x) {
  if (IS_NUMBER(x)) return %_NumberToString(x);
  if (IS_BOOLEAN(x)) return x ? 'true' : 'false';
  if (IS_UNDEFINED(x)) return 'undefined';
  if (IS_SYMBOL(x)) throw MakeTypeError(kSymbolToString);
  return (IS_NULL(x)) ? 'null' : ToString(DefaultString(x));
}


// ES6 symbols
function ToName(x) {
  return IS_SYMBOL(x) ? x : ToString(x);
}


// ECMA-262, section 9.9, page 36.
function ToObject(x) {
  if (IS_STRING(x)) return new GlobalString(x);
  if (IS_NUMBER(x)) return new GlobalNumber(x);
  if (IS_BOOLEAN(x)) return new GlobalBoolean(x);
  if (IS_SYMBOL(x)) return %NewSymbolWrapper(x);
  if (IS_NULL_OR_UNDEFINED(x) && !IS_UNDETECTABLE(x)) {
    throw MakeTypeError(kUndefinedOrNullToObject);
  }
  return x;
}


// ECMA-262, section 9.4, page 34.
function ToInteger(x) {
  if (%_IsSmi(x)) return x;
  return %NumberToInteger(ToNumber(x));
}


// ES6, draft 08-24-14, section 7.1.15
function ToLength(arg) {
  arg = ToInteger(arg);
  if (arg < 0) return 0;
  return arg < GlobalNumber.MAX_SAFE_INTEGER ? arg
                                             : GlobalNumber.MAX_SAFE_INTEGER;
}


// ECMA-262, section 9.6, page 34.
function ToUint32(x) {
  if (%_IsSmi(x) && x >= 0) return x;
  return %NumberToJSUint32(ToNumber(x));
}


// ECMA-262, section 9.5, page 34
function ToInt32(x) {
  if (%_IsSmi(x)) return x;
  return %NumberToJSInt32(ToNumber(x));
}


// ES5, section 9.12
function SameValue(x, y) {
  if (typeof x != typeof y) return false;
  if (IS_NUMBER(x)) {
    if (NUMBER_IS_NAN(x) && NUMBER_IS_NAN(y)) return true;
    // x is +0 and y is -0 or vice versa.
    if (x === 0 && y === 0 && %_IsMinusZero(x) != %_IsMinusZero(y)) {
      return false;
    }
  }
  return x === y;
}


// ES6, section 7.2.4
function SameValueZero(x, y) {
  if (typeof x != typeof y) return false;
  if (IS_NUMBER(x)) {
    if (NUMBER_IS_NAN(x) && NUMBER_IS_NAN(y)) return true;
  }
  return x === y;
}

function ConcatIterableToArray(target, iterable) {
   var index = target.length;
   for (var element of iterable) {
     %AddElement(target, index++, element);
   }
   return target;
}


/* ---------------------------------
   - - -   U t i l i t i e s   - - -
   ---------------------------------
*/

// Returns if the given x is a primitive value - not an object or a
// function.
function IsPrimitive(x) {
  // Even though the type of null is "object", null is still
  // considered a primitive value. IS_SPEC_OBJECT handles this correctly
  // (i.e., it will return false if x is null).
  return !IS_SPEC_OBJECT(x);
}


// ES6, draft 10-14-14, section 22.1.3.1.1
function IsConcatSpreadable(O) {
  if (!IS_SPEC_OBJECT(O)) return false;
  var spreadable = O[symbolIsConcatSpreadable];
  if (IS_UNDEFINED(spreadable)) return IS_ARRAY(O);
  return ToBoolean(spreadable);
}


// ECMA-262, section 8.6.2.6, page 28.
function DefaultNumber(x) {
  if (!IS_SYMBOL_WRAPPER(x)) {
    var valueOf = x.valueOf;
    if (IS_SPEC_FUNCTION(valueOf)) {
      var v = %_CallFunction(x, valueOf);
      if (IsPrimitive(v)) return v;
    }

    var toString = x.toString;
    if (IS_SPEC_FUNCTION(toString)) {
      var s = %_CallFunction(x, toString);
      if (IsPrimitive(s)) return s;
    }
  }
  throw MakeTypeError(kCannotConvertToPrimitive);
}

// ECMA-262, section 8.6.2.6, page 28.
function DefaultString(x) {
  if (!IS_SYMBOL_WRAPPER(x)) {
    var toString = x.toString;
    if (IS_SPEC_FUNCTION(toString)) {
      var s = %_CallFunction(x, toString);
      if (IsPrimitive(s)) return s;
    }

    var valueOf = x.valueOf;
    if (IS_SPEC_FUNCTION(valueOf)) {
      var v = %_CallFunction(x, valueOf);
      if (IsPrimitive(v)) return v;
    }
  }
  throw MakeTypeError(kCannotConvertToPrimitive);
}

function ToPositiveInteger(x, rangeErrorIndex) {
  var i = TO_INTEGER_MAP_MINUS_ZERO(x);
  if (i < 0) throw MakeRangeError(rangeErrorIndex);
  return i;
}

//----------------------------------------------------------------------------

// NOTE: Setting the prototype for Array must take place as early as
// possible due to code generation for array literals.  When
// generating code for a array literal a boilerplate array is created
// that is cloned when running the code.  It is essential that the
// boilerplate gets the right prototype.
%FunctionSetPrototype(GlobalArray, new GlobalArray(0));

//----------------------------------------------------------------------------

$concatIterableToArray = ConcatIterableToArray;
$defaultNumber = DefaultNumber;
$defaultString = DefaultString;
$NaN = %GetRootNaN();
$nonNumberToNumber = NonNumberToNumber;
$nonStringToString = NonStringToString;
$sameValue = SameValue;
$sameValueZero = SameValueZero;
$toBoolean = ToBoolean;
$toInt32 = ToInt32;
$toInteger = ToInteger;
$toLength = ToLength;
$toName = ToName;
$toNumber = ToNumber;
$toObject = ToObject;
$toPositiveInteger = ToPositiveInteger;
$toPrimitive = ToPrimitive;
$toString = ToString;
$toUint32 = ToUint32;

})
