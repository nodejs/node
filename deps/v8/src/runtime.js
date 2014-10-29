// Copyright 2006-2008 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This files contains runtime support implemented in JavaScript.

// CAUTION: Some of the functions specified in this file are called
// directly from compiled code. These are the functions with names in
// ALL CAPS. The compiled code passes the first argument in 'this' and
// it does not push the function onto the stack. This means that you
// cannot use contexts in all these functions.


/* -----------------------------------
   - - -   C o m p a r i s o n   - - -
   -----------------------------------
*/

// The following declarations are shared with other native JS files.
// They are all declared at this one spot to avoid redeclaration errors.
var $Object = global.Object;
var $Array = global.Array;
var $String = global.String;
var $Number = global.Number;
var $Function = global.Function;
var $Boolean = global.Boolean;
var $NaN = %GetRootNaN();

// ECMA-262 Section 11.9.3.
function EQUALS(y) {
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
          return %NumberEquals(x, %ToNumber(y));
        }
        y = %ToPrimitive(y, NO_HINT);
      }
    } else if (IS_STRING(x)) {
      while (true) {
        if (IS_STRING(y)) return %StringEquals(x, y);
        if (IS_SYMBOL(y)) return 1;  // not equal
        if (IS_NUMBER(y)) return %NumberEquals(%ToNumber(x), y);
        if (IS_BOOLEAN(y)) return %NumberEquals(%ToNumber(x), %ToNumber(y));
        if (IS_NULL_OR_UNDEFINED(y)) return 1;  // not equal
        y = %ToPrimitive(y, NO_HINT);
      }
    } else if (IS_SYMBOL(x)) {
      if (IS_SYMBOL(y)) return %_ObjectEquals(x, y) ? 0 : 1;
      return 1; // not equal
    } else if (IS_BOOLEAN(x)) {
      if (IS_BOOLEAN(y)) return %_ObjectEquals(x, y) ? 0 : 1;
      if (IS_NULL_OR_UNDEFINED(y)) return 1;
      if (IS_NUMBER(y)) return %NumberEquals(%ToNumber(x), y);
      if (IS_STRING(y)) return %NumberEquals(%ToNumber(x), %ToNumber(y));
      if (IS_SYMBOL(y)) return 1;  // not equal
      // y is object.
      x = %ToNumber(x);
      y = %ToPrimitive(y, NO_HINT);
    } else if (IS_NULL_OR_UNDEFINED(x)) {
      return IS_NULL_OR_UNDEFINED(y) ? 0 : 1;
    } else {
      // x is an object.
      if (IS_SPEC_OBJECT(y)) {
        return %_ObjectEquals(x, y) ? 0 : 1;
      }
      if (IS_NULL_OR_UNDEFINED(y)) return 1;  // not equal
      if (IS_SYMBOL(y)) return 1;  // not equal
      if (IS_BOOLEAN(y)) y = %ToNumber(y);
      x = %ToPrimitive(x, NO_HINT);
    }
  }
}

// ECMA-262, section 11.9.4, page 56.
function STRICT_EQUALS(x) {
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
function COMPARE(x, ncr) {
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
      %ToPrimitive(x, NUMBER_HINT);
    }
    return ncr;
  } else if (IS_UNDEFINED(x)) {
    %ToPrimitive(this, NUMBER_HINT);
    return ncr;
  } else {
    left = %ToPrimitive(this, NUMBER_HINT);
  }

  right = %ToPrimitive(x, NUMBER_HINT);
  if (IS_STRING(left) && IS_STRING(right)) {
    return %_StringCompare(left, right);
  } else {
    var left_number = %ToNumber(left);
    var right_number = %ToNumber(right);
    if (NUMBER_IS_NAN(left_number) || NUMBER_IS_NAN(right_number)) return ncr;
    return %NumberCompare(left_number, right_number, ncr);
  }
}



/* -----------------------------------
   - - -   A r i t h m e t i c   - - -
   -----------------------------------
*/

// ECMA-262, section 11.6.1, page 50.
function ADD(x) {
  // Fast case: Check for number operands and do the addition.
  if (IS_NUMBER(this) && IS_NUMBER(x)) return %NumberAdd(this, x);
  if (IS_STRING(this) && IS_STRING(x)) return %_StringAdd(this, x);

  // Default implementation.
  var a = %ToPrimitive(this, NO_HINT);
  var b = %ToPrimitive(x, NO_HINT);

  if (IS_STRING(a)) {
    return %_StringAdd(a, %ToString(b));
  } else if (IS_STRING(b)) {
    return %_StringAdd(%NonStringToString(a), b);
  } else {
    return %NumberAdd(%ToNumber(a), %ToNumber(b));
  }
}


// Left operand (this) is already a string.
function STRING_ADD_LEFT(y) {
  if (!IS_STRING(y)) {
    if (IS_STRING_WRAPPER(y) && %_IsStringWrapperSafeForDefaultValueOf(y)) {
      y = %_ValueOf(y);
    } else {
      y = IS_NUMBER(y)
          ? %_NumberToString(y)
          : %ToString(%ToPrimitive(y, NO_HINT));
    }
  }
  return %_StringAdd(this, y);
}


// Right operand (y) is already a string.
function STRING_ADD_RIGHT(y) {
  var x = this;
  if (!IS_STRING(x)) {
    if (IS_STRING_WRAPPER(x) && %_IsStringWrapperSafeForDefaultValueOf(x)) {
      x = %_ValueOf(x);
    } else {
      x = IS_NUMBER(x)
          ? %_NumberToString(x)
          : %ToString(%ToPrimitive(x, NO_HINT));
    }
  }
  return %_StringAdd(x, y);
}


// ECMA-262, section 11.6.2, page 50.
function SUB(y) {
  var x = IS_NUMBER(this) ? this : %NonNumberToNumber(this);
  if (!IS_NUMBER(y)) y = %NonNumberToNumber(y);
  return %NumberSub(x, y);
}


// ECMA-262, section 11.5.1, page 48.
function MUL(y) {
  var x = IS_NUMBER(this) ? this : %NonNumberToNumber(this);
  if (!IS_NUMBER(y)) y = %NonNumberToNumber(y);
  return %NumberMul(x, y);
}


// ECMA-262, section 11.5.2, page 49.
function DIV(y) {
  var x = IS_NUMBER(this) ? this : %NonNumberToNumber(this);
  if (!IS_NUMBER(y)) y = %NonNumberToNumber(y);
  return %NumberDiv(x, y);
}


// ECMA-262, section 11.5.3, page 49.
function MOD(y) {
  var x = IS_NUMBER(this) ? this : %NonNumberToNumber(this);
  if (!IS_NUMBER(y)) y = %NonNumberToNumber(y);
  return %NumberMod(x, y);
}



/* -------------------------------------------
   - - -   B i t   o p e r a t i o n s   - - -
   -------------------------------------------
*/

// ECMA-262, section 11.10, page 57.
function BIT_OR(y) {
  var x = IS_NUMBER(this) ? this : %NonNumberToNumber(this);
  if (!IS_NUMBER(y)) y = %NonNumberToNumber(y);
  return %NumberOr(x, y);
}


// ECMA-262, section 11.10, page 57.
function BIT_AND(y) {
  var x;
  if (IS_NUMBER(this)) {
    x = this;
    if (!IS_NUMBER(y)) y = %NonNumberToNumber(y);
  } else {
    x = %NonNumberToNumber(this);
    // Make sure to convert the right operand to a number before
    // bailing out in the fast case, but after converting the
    // left operand. This ensures that valueOf methods on the right
    // operand are always executed.
    if (!IS_NUMBER(y)) y = %NonNumberToNumber(y);
    // Optimize for the case where we end up AND'ing a value
    // that doesn't convert to a number. This is common in
    // certain benchmarks.
    if (NUMBER_IS_NAN(x)) return 0;
  }
  return %NumberAnd(x, y);
}


// ECMA-262, section 11.10, page 57.
function BIT_XOR(y) {
  var x = IS_NUMBER(this) ? this : %NonNumberToNumber(this);
  if (!IS_NUMBER(y)) y = %NonNumberToNumber(y);
  return %NumberXor(x, y);
}


// ECMA-262, section 11.7.1, page 51.
function SHL(y) {
  var x = IS_NUMBER(this) ? this : %NonNumberToNumber(this);
  if (!IS_NUMBER(y)) y = %NonNumberToNumber(y);
  return %NumberShl(x, y);
}


// ECMA-262, section 11.7.2, page 51.
function SAR(y) {
  var x;
  if (IS_NUMBER(this)) {
    x = this;
    if (!IS_NUMBER(y)) y = %NonNumberToNumber(y);
  } else {
    x = %NonNumberToNumber(this);
    // Make sure to convert the right operand to a number before
    // bailing out in the fast case, but after converting the
    // left operand. This ensures that valueOf methods on the right
    // operand are always executed.
    if (!IS_NUMBER(y)) y = %NonNumberToNumber(y);
    // Optimize for the case where we end up shifting a value
    // that doesn't convert to a number. This is common in
    // certain benchmarks.
    if (NUMBER_IS_NAN(x)) return 0;
  }
  return %NumberSar(x, y);
}


// ECMA-262, section 11.7.3, page 52.
function SHR(y) {
  var x = IS_NUMBER(this) ? this : %NonNumberToNumber(this);
  if (!IS_NUMBER(y)) y = %NonNumberToNumber(y);
  return %NumberShr(x, y);
}



/* -----------------------------
   - - -   H e l p e r s   - - -
   -----------------------------
*/

// ECMA-262, section 11.4.1, page 46.
function DELETE(key, strict) {
  return %DeleteProperty(%ToObject(this), %ToName(key), strict);
}


// ECMA-262, section 11.8.7, page 54.
function IN(x) {
  if (!IS_SPEC_OBJECT(x)) {
    throw %MakeTypeError('invalid_in_operator_use', [this, x]);
  }
  return %_IsNonNegativeSmi(this) ?
    %HasElement(x, this) : %HasProperty(x, %ToName(this));
}


// ECMA-262, section 11.8.6, page 54. To make the implementation more
// efficient, the return value should be zero if the 'this' is an
// instance of F, and non-zero if not. This makes it possible to avoid
// an expensive ToBoolean conversion in the generated code.
function INSTANCE_OF(F) {
  var V = this;
  if (!IS_SPEC_FUNCTION(F)) {
    throw %MakeTypeError('instanceof_function_expected', [F]);
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
    throw %MakeTypeError('instanceof_nonobject_proto', [O]);
  }

  // Return whether or not O is in the prototype chain of V.
  return %IsInPrototypeChain(O, V) ? 0 : 1;
}


// Filter a given key against an object by checking if the object
// has a property with the given key; return the key as a string if
// it has. Otherwise returns 0 (smi). Used in for-in statements.
function FILTER_KEY(key) {
  var string = %ToName(key);
  if (%HasProperty(this, string)) return string;
  return 0;
}


function CALL_NON_FUNCTION() {
  var delegate = %GetFunctionDelegate(this);
  if (!IS_FUNCTION(delegate)) {
    throw %MakeTypeError('called_non_callable', [typeof this]);
  }
  return %Apply(delegate, this, arguments, 0, %_ArgumentsLength());
}


function CALL_NON_FUNCTION_AS_CONSTRUCTOR() {
  var delegate = %GetConstructorDelegate(this);
  if (!IS_FUNCTION(delegate)) {
    throw %MakeTypeError('called_non_callable', [typeof this]);
  }
  return %Apply(delegate, this, arguments, 0, %_ArgumentsLength());
}


function CALL_FUNCTION_PROXY() {
  var arity = %_ArgumentsLength() - 1;
  var proxy = %_Arguments(arity);  // The proxy comes in as an additional arg.
  var trap = %GetCallTrap(proxy);
  return %Apply(trap, this, arguments, 0, arity);
}


function CALL_FUNCTION_PROXY_AS_CONSTRUCTOR() {
  var proxy = this;
  var trap = %GetConstructTrap(proxy);
  return %Apply(trap, this, arguments, 0, %_ArgumentsLength());
}


function APPLY_PREPARE(args) {
  var length;
  // First check whether length is a positive Smi and args is an
  // array. This is the fast case. If this fails, we do the slow case
  // that takes care of more eventualities.
  if (IS_ARRAY(args)) {
    length = args.length;
    if (%_IsSmi(length) && length >= 0 && length < 0x800000 &&
        IS_SPEC_FUNCTION(this)) {
      return length;
    }
  }

  length = (args == null) ? 0 : %ToUint32(args.length);

  // We can handle any number of apply arguments if the stack is
  // big enough, but sanity check the value to avoid overflow when
  // multiplying with pointer size.
  if (length > 0x800000) {
    throw %MakeRangeError('stack_overflow', []);
  }

  if (!IS_SPEC_FUNCTION(this)) {
    throw %MakeTypeError('apply_non_function',
                         [ %ToString(this), typeof this ]);
  }

  // Make sure the arguments list has the right type.
  if (args != null && !IS_SPEC_OBJECT(args)) {
    throw %MakeTypeError('apply_wrong_args', []);
  }

  // Return the length which is the number of arguments to copy to the
  // stack. It is guaranteed to be a small integer at this point.
  return length;
}


function STACK_OVERFLOW(length) {
  throw %MakeRangeError('stack_overflow', []);
}


// Convert the receiver to an object - forward to ToObject.
function TO_OBJECT() {
  return %ToObject(this);
}


// Convert the receiver to a number - forward to ToNumber.
function TO_NUMBER() {
  return %ToNumber(this);
}


// Convert the receiver to a string - forward to ToString.
function TO_STRING() {
  return %ToString(this);
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
  if (IS_SYMBOL_WRAPPER(x)) throw MakeTypeError('symbol_to_primitive', []);
  if (hint == NO_HINT) hint = (IS_DATE(x)) ? STRING_HINT : NUMBER_HINT;
  return (hint == NUMBER_HINT) ? %DefaultNumber(x) : %DefaultString(x);
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
  if (IS_SYMBOL(x)) throw MakeTypeError('symbol_to_number', []);
  return (IS_NULL(x)) ? 0 : ToNumber(%DefaultNumber(x));
}

function NonNumberToNumber(x) {
  if (IS_STRING(x)) {
    return %_HasCachedArrayIndex(x) ? %_GetCachedArrayIndex(x)
                                    : %StringToNumber(x);
  }
  if (IS_BOOLEAN(x)) return x ? 1 : 0;
  if (IS_UNDEFINED(x)) return NAN;
  if (IS_SYMBOL(x)) throw MakeTypeError('symbol_to_number', []);
  return (IS_NULL(x)) ? 0 : ToNumber(%DefaultNumber(x));
}


// ECMA-262, section 9.8, page 35.
function ToString(x) {
  if (IS_STRING(x)) return x;
  if (IS_NUMBER(x)) return %_NumberToString(x);
  if (IS_BOOLEAN(x)) return x ? 'true' : 'false';
  if (IS_UNDEFINED(x)) return 'undefined';
  if (IS_SYMBOL(x)) throw %MakeTypeError('symbol_to_string', []);
  return (IS_NULL(x)) ? 'null' : %ToString(%DefaultString(x));
}

function NonStringToString(x) {
  if (IS_NUMBER(x)) return %_NumberToString(x);
  if (IS_BOOLEAN(x)) return x ? 'true' : 'false';
  if (IS_UNDEFINED(x)) return 'undefined';
  if (IS_SYMBOL(x)) throw %MakeTypeError('symbol_to_string', []);
  return (IS_NULL(x)) ? 'null' : %ToString(%DefaultString(x));
}


// ES6 symbols
function ToName(x) {
  return IS_SYMBOL(x) ? x : %ToString(x);
}


// ECMA-262, section 9.9, page 36.
function ToObject(x) {
  if (IS_STRING(x)) return new $String(x);
  if (IS_NUMBER(x)) return new $Number(x);
  if (IS_BOOLEAN(x)) return new $Boolean(x);
  if (IS_SYMBOL(x)) return %NewSymbolWrapper(x);
  if (IS_NULL_OR_UNDEFINED(x) && !IS_UNDETECTABLE(x)) {
    throw %MakeTypeError('undefined_or_null_to_object', []);
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
  return arg < $Number.MAX_SAFE_INTEGER ? arg : $Number.MAX_SAFE_INTEGER;
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


// ECMA-262, section 8.6.2.6, page 28.
function DefaultNumber(x) {
  if (!IS_SYMBOL_WRAPPER(x)) {
    var valueOf = x.valueOf;
    if (IS_SPEC_FUNCTION(valueOf)) {
      var v = %_CallFunction(x, valueOf);
      if (%IsPrimitive(v)) return v;
    }

    var toString = x.toString;
    if (IS_SPEC_FUNCTION(toString)) {
      var s = %_CallFunction(x, toString);
      if (%IsPrimitive(s)) return s;
    }
  }
  throw %MakeTypeError('cannot_convert_to_primitive', []);
}

// ECMA-262, section 8.6.2.6, page 28.
function DefaultString(x) {
  if (!IS_SYMBOL_WRAPPER(x)) {
    var toString = x.toString;
    if (IS_SPEC_FUNCTION(toString)) {
      var s = %_CallFunction(x, toString);
      if (%IsPrimitive(s)) return s;
    }

    var valueOf = x.valueOf;
    if (IS_SPEC_FUNCTION(valueOf)) {
      var v = %_CallFunction(x, valueOf);
      if (%IsPrimitive(v)) return v;
    }
  }
  throw %MakeTypeError('cannot_convert_to_primitive', []);
}

function ToPositiveInteger(x, rangeErrorName) {
  var i = TO_INTEGER(x);
  if (i < 0) throw MakeRangeError(rangeErrorName);
  return i;
}


// NOTE: Setting the prototype for Array must take place as early as
// possible due to code generation for array literals.  When
// generating code for a array literal a boilerplate array is created
// that is cloned when running the code.  It is essential that the
// boilerplate gets the right prototype.
%FunctionSetPrototype($Array, new $Array(0));
