// Copyright 2006-2008 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This files contains runtime support implemented in JavaScript.

// CAUTION: Some of the functions specified in this file are called
// directly from compiled code. These are the functions with names in
// ALL CAPS. The compiled code passes the first argument in 'this'.


// The following declarations are shared with other native JS files.
// They are all declared at this one spot to avoid redeclaration errors.

(function(global, utils) {

%CheckIsBootstrapping();

var GlobalArray = global.Array;
var GlobalBoolean = global.Boolean;
var GlobalString = global.String;
var isConcatSpreadableSymbol =
    utils.ImportNow("is_concat_spreadable_symbol");
var MakeRangeError;

utils.Import(function(from) {
  MakeRangeError = from.MakeRangeError;
});

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
     AddIndexedProperty(target, index++, element);
   }
   return target;
}


/* ---------------------------------
   - - -   U t i l i t i e s   - - -
   ---------------------------------
*/


// This function should be called rather than %AddElement in contexts where the
// argument might not be less than 2**32-1. ES2015 ToLength semantics mean that
// this is a concern at basically all callsites.
function AddIndexedProperty(obj, index, value) {
  if (index === TO_UINT32(index) && index !== kMaxUint32) {
    %AddElement(obj, index, value);
  } else {
    %AddNamedProperty(obj, TO_STRING(index), value, NONE);
  }
}
%SetForceInlineFlag(AddIndexedProperty);


// ES6, draft 10-14-14, section 22.1.3.1.1
function IsConcatSpreadable(O) {
  if (!IS_SPEC_OBJECT(O)) return false;
  var spreadable = O[isConcatSpreadableSymbol];
  if (IS_UNDEFINED(spreadable)) return IS_ARRAY(O);
  return TO_BOOLEAN(spreadable);
}


function ToPositiveInteger(x, rangeErrorIndex) {
  var i = TO_INTEGER_MAP_MINUS_ZERO(x);
  if (i < 0) throw MakeRangeError(rangeErrorIndex);
  return i;
}


function MaxSimple(a, b) {
  return a > b ? a : b;
}


function MinSimple(a, b) {
  return a > b ? b : a;
}


%SetForceInlineFlag(MaxSimple);
%SetForceInlineFlag(MinSimple);

//----------------------------------------------------------------------------

// NOTE: Setting the prototype for Array must take place as early as
// possible due to code generation for array literals.  When
// generating code for a array literal a boilerplate array is created
// that is cloned when running the code.  It is essential that the
// boilerplate gets the right prototype.
%FunctionSetPrototype(GlobalArray, new GlobalArray(0));

// ----------------------------------------------------------------------------
// Exports

utils.Export(function(to) {
  to.AddIndexedProperty = AddIndexedProperty;
  to.MaxSimple = MaxSimple;
  to.MinSimple = MinSimple;
  to.SameValue = SameValue;
  to.SameValueZero = SameValueZero;
  to.ToPositiveInteger = ToPositiveInteger;
});

%InstallToContext([
  "apply_prepare_builtin", APPLY_PREPARE,
  "concat_iterable_to_array_builtin", CONCAT_ITERABLE_TO_ARRAY,
  "reflect_apply_prepare_builtin", REFLECT_APPLY_PREPARE,
  "reflect_construct_prepare_builtin", REFLECT_CONSTRUCT_PREPARE,
]);

%InstallToContext([
  "concat_iterable_to_array", ConcatIterableToArray,
]);

})
