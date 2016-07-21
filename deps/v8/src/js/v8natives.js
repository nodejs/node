// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function(global, utils) {

%CheckIsBootstrapping();

// ----------------------------------------------------------------------------
// Imports

var GlobalArray = global.Array;
var GlobalNumber = global.Number;
var GlobalObject = global.Object;
var InternalArray = utils.InternalArray;
var iteratorSymbol = utils.ImportNow("iterator_symbol");
var MakeRangeError;
var MakeSyntaxError;
var MakeTypeError;
var MathAbs;
var NaN = %GetRootNaN();
var ObjectToString = utils.ImportNow("object_to_string");
var toStringTagSymbol = utils.ImportNow("to_string_tag_symbol");

utils.Import(function(from) {
  MakeRangeError = from.MakeRangeError;
  MakeSyntaxError = from.MakeSyntaxError;
  MakeTypeError = from.MakeTypeError;
  MathAbs = from.MathAbs;
});

// ----------------------------------------------------------------------------


// ES6 18.2.3 isNaN(number)
function GlobalIsNaN(number) {
  number = TO_NUMBER(number);
  return NUMBER_IS_NAN(number);
}


// ES6 18.2.2 isFinite(number)
function GlobalIsFinite(number) {
  number = TO_NUMBER(number);
  return NUMBER_IS_FINITE(number);
}


// ES6 18.2.5 parseInt(string, radix)
function GlobalParseInt(string, radix) {
  if (IS_UNDEFINED(radix) || radix === 10 || radix === 0) {
    // Some people use parseInt instead of Math.floor.  This
    // optimization makes parseInt on a Smi 12 times faster (60ns
    // vs 800ns).  The following optimization makes parseInt on a
    // non-Smi number 9 times faster (230ns vs 2070ns).  Together
    // they make parseInt on a string 1.4% slower (274ns vs 270ns).
    if (%_IsSmi(string)) return string;
    if (IS_NUMBER(string) &&
        ((0.01 < string && string < 1e9) ||
            (-1e9 < string && string < -0.01))) {
      // Truncate number.
      return string | 0;
    }
    string = TO_STRING(string);
    radix = radix | 0;
  } else {
    // The spec says ToString should be evaluated before ToInt32.
    string = TO_STRING(string);
    radix = TO_INT32(radix);
    if (!(radix == 0 || (2 <= radix && radix <= 36))) {
      return NaN;
    }
  }

  if (%_HasCachedArrayIndex(string) &&
      (radix == 0 || radix == 10)) {
    return %_GetCachedArrayIndex(string);
  }
  return %StringParseInt(string, radix);
}


// ES6 18.2.4 parseFloat(string)
function GlobalParseFloat(string) {
  // 1. Let inputString be ? ToString(string).
  string = TO_STRING(string);
  if (%_HasCachedArrayIndex(string)) return %_GetCachedArrayIndex(string);
  return %StringParseFloat(string);
}


// ----------------------------------------------------------------------------

// Set up global object.
var attributes = DONT_ENUM | DONT_DELETE | READ_ONLY;

utils.InstallConstants(global, [
  // ES6 18.1.1
  "Infinity", INFINITY,
  // ES6 18.1.2
  "NaN", NaN,
  // ES6 18.1.3
  "undefined", UNDEFINED,
]);

// Set up non-enumerable function on the global object.
utils.InstallFunctions(global, DONT_ENUM, [
  "isNaN", GlobalIsNaN,
  "isFinite", GlobalIsFinite,
  "parseInt", GlobalParseInt,
  "parseFloat", GlobalParseFloat,
]);


// ----------------------------------------------------------------------------
// Object

// ES6 19.1.3.5 Object.prototype.toLocaleString([reserved1 [,reserved2]])
function ObjectToLocaleString() {
  CHECK_OBJECT_COERCIBLE(this, "Object.prototype.toLocaleString");
  return this.toString();
}


// ES6 19.1.3.7 Object.prototype.valueOf()
function ObjectValueOf() {
  return TO_OBJECT(this);
}


// ES6 19.1.3.3 Object.prototype.isPrototypeOf(V)
function ObjectIsPrototypeOf(V) {
  if (!IS_RECEIVER(V)) return false;
  var O = TO_OBJECT(this);
  return %HasInPrototypeChain(V, O);
}


// ES6 19.1.3.4
function ObjectPropertyIsEnumerable(V) {
  var P = TO_NAME(V);
  return %PropertyIsEnumerable(TO_OBJECT(this), P);
}

// ES6 7.3.9
function GetMethod(obj, p) {
  var func = obj[p];
  if (IS_NULL_OR_UNDEFINED(func)) return UNDEFINED;
  if (IS_CALLABLE(func)) return func;
  throw MakeTypeError(kCalledNonCallable, typeof func);
}

// ES6 section 19.1.2.18.
function ObjectSetPrototypeOf(obj, proto) {
  CHECK_OBJECT_COERCIBLE(obj, "Object.setPrototypeOf");

  if (proto !== null && !IS_RECEIVER(proto)) {
    throw MakeTypeError(kProtoObjectOrNull, proto);
  }

  if (IS_RECEIVER(obj)) {
    %SetPrototype(obj, proto);
  }

  return obj;
}

// ES6 B.2.2.1.1
function ObjectGetProto() {
  return %object_get_prototype_of(this);
}


// ES6 B.2.2.1.2
function ObjectSetProto(proto) {
  CHECK_OBJECT_COERCIBLE(this, "Object.prototype.__proto__");

  if ((IS_RECEIVER(proto) || IS_NULL(proto)) && IS_RECEIVER(this)) {
    %SetPrototype(this, proto);
  }
}


// ES6 19.1.1.1
function ObjectConstructor(x) {
  if (GlobalObject != new.target && !IS_UNDEFINED(new.target)) {
    return this;
  }
  if (IS_NULL(x) || IS_UNDEFINED(x)) return {};
  return TO_OBJECT(x);
}


// ----------------------------------------------------------------------------
// Object

%SetNativeFlag(GlobalObject);
%SetCode(GlobalObject, ObjectConstructor);

%AddNamedProperty(GlobalObject.prototype, "constructor", GlobalObject,
                  DONT_ENUM);

// Set up non-enumerable functions on the Object.prototype object.
utils.InstallFunctions(GlobalObject.prototype, DONT_ENUM, [
  "toString", ObjectToString,
  "toLocaleString", ObjectToLocaleString,
  "valueOf", ObjectValueOf,
  "isPrototypeOf", ObjectIsPrototypeOf,
  "propertyIsEnumerable", ObjectPropertyIsEnumerable,
  // __defineGetter__ is added in bootstrapper.cc.
  // __lookupGetter__ is added in bootstrapper.cc.
  // __defineSetter__ is added in bootstrapper.cc.
  // __lookupSetter__ is added in bootstrapper.cc.
]);
utils.InstallGetterSetter(
    GlobalObject.prototype, "__proto__", ObjectGetProto, ObjectSetProto);

// Set up non-enumerable functions in the Object object.
utils.InstallFunctions(GlobalObject, DONT_ENUM, [
  "setPrototypeOf", ObjectSetPrototypeOf,
  // getOwnPropertySymbols is added in symbol.js.
  // Others are added in bootstrapper.cc.
]);



// ----------------------------------------------------------------------------
// Number

// ES6 Number.prototype.toString([ radix ])
function NumberToStringJS(radix) {
  // NOTE: Both Number objects and values can enter here as
  // 'this'. This is not as dictated by ECMA-262.
  var number = this;
  if (!IS_NUMBER(this)) {
    if (!IS_NUMBER_WRAPPER(this)) {
      throw MakeTypeError(kNotGeneric, 'Number.prototype.toString');
    }
    // Get the value of this number in case it's an object.
    number = %_ValueOf(this);
  }
  // Fast case: Convert number in radix 10.
  if (IS_UNDEFINED(radix) || radix === 10) {
    return %_NumberToString(number);
  }

  // Convert the radix to an integer and check the range.
  radix = TO_INTEGER(radix);
  if (radix < 2 || radix > 36) throw MakeRangeError(kToRadixFormatRange);
  // Convert the number to a string in the given radix.
  return %NumberToRadixString(number, radix);
}


// ES6 20.1.3.4 Number.prototype.toLocaleString([reserved1 [, reserved2]])
function NumberToLocaleString() {
  return %_Call(NumberToStringJS, this);
}


// ES6 20.1.3.7 Number.prototype.valueOf()
function NumberValueOf() {
  // NOTE: Both Number objects and values can enter here as
  // 'this'. This is not as dictated by ECMA-262.
  if (!IS_NUMBER(this) && !IS_NUMBER_WRAPPER(this)) {
    throw MakeTypeError(kNotGeneric, 'Number.prototype.valueOf');
  }
  return %_ValueOf(this);
}


// ES6 20.1.3.3 Number.prototype.toFixed(fractionDigits)
function NumberToFixedJS(fractionDigits) {
  var x = this;
  if (!IS_NUMBER(this)) {
    if (!IS_NUMBER_WRAPPER(this)) {
      throw MakeTypeError(kIncompatibleMethodReceiver,
                          "Number.prototype.toFixed", this);
    }
    // Get the value of this number in case it's an object.
    x = %_ValueOf(this);
  }
  var f = TO_INTEGER(fractionDigits);

  if (f < 0 || f > 20) {
    throw MakeRangeError(kNumberFormatRange, "toFixed() digits");
  }

  if (NUMBER_IS_NAN(x)) return "NaN";
  if (x == INFINITY) return "Infinity";
  if (x == -INFINITY) return "-Infinity";

  return %NumberToFixed(x, f);
}


// ES6 20.1.3.2 Number.prototype.toExponential(fractionDigits)
function NumberToExponentialJS(fractionDigits) {
  var x = this;
  if (!IS_NUMBER(this)) {
    if (!IS_NUMBER_WRAPPER(this)) {
      throw MakeTypeError(kIncompatibleMethodReceiver,
                          "Number.prototype.toExponential", this);
    }
    // Get the value of this number in case it's an object.
    x = %_ValueOf(this);
  }
  var f = IS_UNDEFINED(fractionDigits) ? UNDEFINED : TO_INTEGER(fractionDigits);

  if (NUMBER_IS_NAN(x)) return "NaN";
  if (x == INFINITY) return "Infinity";
  if (x == -INFINITY) return "-Infinity";

  if (IS_UNDEFINED(f)) {
    f = -1;  // Signal for runtime function that f is not defined.
  } else if (f < 0 || f > 20) {
    throw MakeRangeError(kNumberFormatRange, "toExponential()");
  }
  return %NumberToExponential(x, f);
}


// ES6 20.1.3.5 Number.prototype.toPrecision(precision)
function NumberToPrecisionJS(precision) {
  var x = this;
  if (!IS_NUMBER(this)) {
    if (!IS_NUMBER_WRAPPER(this)) {
      throw MakeTypeError(kIncompatibleMethodReceiver,
                          "Number.prototype.toPrecision", this);
    }
    // Get the value of this number in case it's an object.
    x = %_ValueOf(this);
  }
  if (IS_UNDEFINED(precision)) return TO_STRING(x);
  var p = TO_INTEGER(precision);

  if (NUMBER_IS_NAN(x)) return "NaN";
  if (x == INFINITY) return "Infinity";
  if (x == -INFINITY) return "-Infinity";

  if (p < 1 || p > 21) {
    throw MakeRangeError(kToPrecisionFormatRange);
  }
  return %NumberToPrecision(x, p);
}


// Harmony isFinite.
function NumberIsFinite(number) {
  return IS_NUMBER(number) && NUMBER_IS_FINITE(number);
}


// Harmony isInteger
function NumberIsInteger(number) {
  return NumberIsFinite(number) && TO_INTEGER(number) == number;
}


// Harmony isNaN.
function NumberIsNaN(number) {
  return IS_NUMBER(number) && NUMBER_IS_NAN(number);
}


// Harmony isSafeInteger
function NumberIsSafeInteger(number) {
  if (NumberIsFinite(number)) {
    var integral = TO_INTEGER(number);
    if (integral == number) {
      return MathAbs(integral) <= kMaxSafeInteger;
    }
  }
  return false;
}


// ----------------------------------------------------------------------------

%FunctionSetPrototype(GlobalNumber, new GlobalNumber(0));

%OptimizeObjectForAddingMultipleProperties(GlobalNumber.prototype, 8);
// Set up the constructor property on the Number prototype object.
%AddNamedProperty(GlobalNumber.prototype, "constructor", GlobalNumber,
                  DONT_ENUM);

utils.InstallConstants(GlobalNumber, [
  // ECMA-262 section 15.7.3.1.
  "MAX_VALUE", 1.7976931348623157e+308,
  // ECMA-262 section 15.7.3.2.
  "MIN_VALUE", 5e-324,
  // ECMA-262 section 15.7.3.3.
  "NaN", NaN,
  // ECMA-262 section 15.7.3.4.
  "NEGATIVE_INFINITY", -INFINITY,
  // ECMA-262 section 15.7.3.5.
  "POSITIVE_INFINITY", INFINITY,

  // --- Harmony constants (no spec refs until settled.)

  "MAX_SAFE_INTEGER", %_MathPow(2, 53) - 1,
  "MIN_SAFE_INTEGER", -%_MathPow(2, 53) + 1,
  "EPSILON", %_MathPow(2, -52)
]);

// Set up non-enumerable functions on the Number prototype object.
utils.InstallFunctions(GlobalNumber.prototype, DONT_ENUM, [
  "toString", NumberToStringJS,
  "toLocaleString", NumberToLocaleString,
  "valueOf", NumberValueOf,
  "toFixed", NumberToFixedJS,
  "toExponential", NumberToExponentialJS,
  "toPrecision", NumberToPrecisionJS
]);

// Harmony Number constructor additions
utils.InstallFunctions(GlobalNumber, DONT_ENUM, [
  "isFinite", NumberIsFinite,
  "isInteger", NumberIsInteger,
  "isNaN", NumberIsNaN,
  "isSafeInteger", NumberIsSafeInteger,
  "parseInt", GlobalParseInt,
  "parseFloat", GlobalParseFloat
]);

%SetForceInlineFlag(NumberIsNaN);


// ----------------------------------------------------------------------------
// Iterator related spec functions.

// ES6 7.4.1 GetIterator(obj, method)
function GetIterator(obj, method) {
  if (IS_UNDEFINED(method)) {
    method = obj[iteratorSymbol];
  }
  if (!IS_CALLABLE(method)) {
    throw MakeTypeError(kNotIterable, obj);
  }
  var iterator = %_Call(method, obj);
  if (!IS_RECEIVER(iterator)) {
    throw MakeTypeError(kNotAnIterator, iterator);
  }
  return iterator;
}

// ----------------------------------------------------------------------------
// Exports

utils.Export(function(to) {
  to.GetIterator = GetIterator;
  to.GetMethod = GetMethod;
  to.IsFinite = GlobalIsFinite;
  to.IsNaN = GlobalIsNaN;
  to.NumberIsNaN = NumberIsNaN;
  to.NumberIsInteger = NumberIsInteger;
  to.ObjectHasOwnProperty = GlobalObject.prototype.hasOwnProperty;
});

%InstallToContext([
  "object_value_of", ObjectValueOf,
]);

})
