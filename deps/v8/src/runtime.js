// Copyright 2006-2008 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This files contains runtime support implemented in JavaScript.

// CAUTION: Some of the functions specified in this file are called
// directly from compiled code. These are the functions with names in
// ALL CAPS. The compiled code passes the first argument in 'this'.


// The following declarations are shared with other native JS files.
// They are all declared at this one spot to avoid redeclaration errors.
var $NaN;
var $nonNumberToNumber;
var $sameValue;
var $sameValueZero;
var $toNumber;
var $toPositiveInteger;

var harmony_tolength = false;

(function(global, utils) {

%CheckIsBootstrapping();

var GlobalArray = global.Array;
var GlobalBoolean = global.Boolean;
var GlobalString = global.String;
var isConcatSpreadableSymbol =
    utils.ImportNow("is_concat_spreadable_symbol");

// ----------------------------------------------------------------------------

/* -----------------------------
   - - -   H e l p e r s   - - -
   -----------------------------
*/

function APPLY_PREPARE(args) {
  var length;

  // First check that the receiver is callable.
  if (!IS_CALLABLE(this)) {
    throw %make_type_error(kApplyNonFunction, TO_STRING(this), typeof this);
  }

  // First check whether length is a positive Smi and args is an
  // array. This is the fast case. If this fails, we do the slow case
  // that takes care of more eventualities.
  if (IS_ARRAY(args)) {
    length = args.length;
    if (%_IsSmi(length) && length >= 0 && length < kSafeArgumentsLength) {
      return length;
    }
  }

  length = (args == null) ? 0 : TO_UINT32(args.length);

  // We can handle any number of apply arguments if the stack is
  // big enough, but sanity check the value to avoid overflow when
  // multiplying with pointer size.
  if (length > kSafeArgumentsLength) throw %make_range_error(kStackOverflow);

  // Make sure the arguments list has the right type.
  if (args != null && !IS_SPEC_OBJECT(args)) {
    throw %make_type_error(kWrongArgs, "Function.prototype.apply");
  }

  // Return the length which is the number of arguments to copy to the
  // stack. It is guaranteed to be a small integer at this point.
  return length;
}


function REFLECT_APPLY_PREPARE(args) {
  var length;

  // First check that the receiver is callable.
  if (!IS_CALLABLE(this)) {
    throw %make_type_error(kApplyNonFunction, TO_STRING(this), typeof this);
  }

  // First check whether length is a positive Smi and args is an
  // array. This is the fast case. If this fails, we do the slow case
  // that takes care of more eventualities.
  if (IS_ARRAY(args)) {
    length = args.length;
    if (%_IsSmi(length) && length >= 0 && length < kSafeArgumentsLength) {
      return length;
    }
  }

  if (!IS_SPEC_OBJECT(args)) {
    throw %make_type_error(kWrongArgs, "Reflect.apply");
  }

  length = TO_LENGTH(args.length);

  // We can handle any number of apply arguments if the stack is
  // big enough, but sanity check the value to avoid overflow when
  // multiplying with pointer size.
  if (length > kSafeArgumentsLength) throw %make_range_error(kStackOverflow);

  // Return the length which is the number of arguments to copy to the
  // stack. It is guaranteed to be a small integer at this point.
  return length;
}


function REFLECT_CONSTRUCT_PREPARE(
    args, newTarget) {
  var length;
  var ctorOk = IS_CALLABLE(this) && %IsConstructor(this);
  var newTargetOk = IS_CALLABLE(newTarget) && %IsConstructor(newTarget);

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
    if (!IS_CALLABLE(this)) {
      throw %make_type_error(kCalledNonCallable, TO_STRING(this));
    } else {
      throw %make_type_error(kNotConstructor, TO_STRING(this));
    }
  }

  if (!newTargetOk) {
    if (!IS_CALLABLE(newTarget)) {
      throw %make_type_error(kCalledNonCallable, TO_STRING(newTarget));
    } else {
      throw %make_type_error(kNotConstructor, TO_STRING(newTarget));
    }
  }

  if (!IS_SPEC_OBJECT(args)) {
    throw %make_type_error(kWrongArgs, "Reflect.construct");
  }

  length = TO_LENGTH(args.length);

  // We can handle any number of apply arguments if the stack is
  // big enough, but sanity check the value to avoid overflow when
  // multiplying with pointer size.
  if (length > kSafeArgumentsLength) throw %make_range_error(kStackOverflow);

  // Return the length which is the number of arguments to copy to the
  // stack. It is guaranteed to be a small integer at this point.
  return length;
}


function CONCAT_ITERABLE_TO_ARRAY(iterable) {
  return %concat_iterable_to_array(this, iterable);
};


/* -------------------------------------
   - - -   C o n v e r s i o n s   - - -
   -------------------------------------
*/

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
  // Types that can't be converted to number are caught in DefaultNumber.
  return (IS_NULL(x)) ? 0 : ToNumber(DefaultNumber(x));
}

function NonNumberToNumber(x) {
  if (IS_STRING(x)) {
    return %_HasCachedArrayIndex(x) ? %_GetCachedArrayIndex(x)
                                    : %StringToNumber(x);
  }
  if (IS_BOOLEAN(x)) return x ? 1 : 0;
  if (IS_UNDEFINED(x)) return NAN;
  // Types that can't be converted to number are caught in DefaultNumber.
  return (IS_NULL(x)) ? 0 : ToNumber(DefaultNumber(x));
}


// ECMA-262, section 9.8, page 35.
function ToString(x) {
  if (IS_STRING(x)) return x;
  if (IS_NUMBER(x)) return %_NumberToString(x);
  if (IS_BOOLEAN(x)) return x ? 'true' : 'false';
  if (IS_UNDEFINED(x)) return 'undefined';
  // Types that can't be converted to string are caught in DefaultString.
  return (IS_NULL(x)) ? 'null' : ToString(DefaultString(x));
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
  if (IS_SIMD_VALUE(x)) return %SimdSameValue(x, y);
  return x === y;
}


// ES6, section 7.2.4
function SameValueZero(x, y) {
  if (typeof x != typeof y) return false;
  if (IS_NUMBER(x)) {
    if (NUMBER_IS_NAN(x) && NUMBER_IS_NAN(y)) return true;
  }
  if (IS_SIMD_VALUE(x)) return %SimdSameValueZero(x, y);
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
  var spreadable = O[isConcatSpreadableSymbol];
  if (IS_UNDEFINED(spreadable)) return IS_ARRAY(O);
  return ToBoolean(spreadable);
}


// ECMA-262, section 8.6.2.6, page 28.
function DefaultNumber(x) {
  var valueOf = x.valueOf;
  if (IS_CALLABLE(valueOf)) {
    var v = %_Call(valueOf, x);
    if (IS_SYMBOL(v)) throw MakeTypeError(kSymbolToNumber);
    if (IS_SIMD_VALUE(x)) throw MakeTypeError(kSimdToNumber);
    if (IsPrimitive(v)) return v;
  }
  var toString = x.toString;
  if (IS_CALLABLE(toString)) {
    var s = %_Call(toString, x);
    if (IsPrimitive(s)) return s;
  }
  throw MakeTypeError(kCannotConvertToPrimitive);
}

// ECMA-262, section 8.6.2.6, page 28.
function DefaultString(x) {
  if (!IS_SYMBOL_WRAPPER(x)) {
    if (IS_SYMBOL(x)) throw MakeTypeError(kSymbolToString);
    var toString = x.toString;
    if (IS_CALLABLE(toString)) {
      var s = %_Call(toString, x);
      if (IsPrimitive(s)) return s;
    }

    var valueOf = x.valueOf;
    if (IS_CALLABLE(valueOf)) {
      var v = %_Call(valueOf, x);
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

// ----------------------------------------------------------------------------
// Exports

$NaN = %GetRootNaN();
$nonNumberToNumber = NonNumberToNumber;
$sameValue = SameValue;
$sameValueZero = SameValueZero;
$toNumber = ToNumber;
$toPositiveInteger = ToPositiveInteger;

%InstallToContext([
  "apply_prepare_builtin", APPLY_PREPARE,
  "concat_iterable_to_array_builtin", CONCAT_ITERABLE_TO_ARRAY,
  "reflect_apply_prepare_builtin", REFLECT_APPLY_PREPARE,
  "reflect_construct_prepare_builtin", REFLECT_CONSTRUCT_PREPARE,
]);

%InstallToContext([
  "concat_iterable_to_array", ConcatIterableToArray,
  "non_number_to_number", NonNumberToNumber,
  "to_number_fun", ToNumber,
]);

utils.Export(function(to) {
  to.ToBoolean = ToBoolean;
  to.ToNumber = ToNumber;
  to.ToString = ToString;
});

})
