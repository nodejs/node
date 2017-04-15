// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function(global, utils) {

%CheckIsBootstrapping();

// ----------------------------------------------------------------------------
// Imports

var GlobalNumber = global.Number;
var GlobalObject = global.Object;
var iteratorSymbol = utils.ImportNow("iterator_symbol");
var NaN = %GetRootNaN();
var ObjectToString = utils.ImportNow("object_to_string");

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


// ES6 7.3.9
function GetMethod(obj, p) {
  var func = obj[p];
  if (IS_NULL_OR_UNDEFINED(func)) return UNDEFINED;
  if (IS_CALLABLE(func)) return func;
  throw %make_type_error(kCalledNonCallable, typeof func);
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
  // propertyIsEnumerable is added in bootstrapper.cc.
  // __defineGetter__ is added in bootstrapper.cc.
  // __lookupGetter__ is added in bootstrapper.cc.
  // __defineSetter__ is added in bootstrapper.cc.
  // __lookupSetter__ is added in bootstrapper.cc.
]);


// ----------------------------------------------------------------------------
// Number

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

  "MAX_SAFE_INTEGER", 9007199254740991,
  "MIN_SAFE_INTEGER", -9007199254740991,
  "EPSILON", 2.220446049250313e-16,
]);


// ----------------------------------------------------------------------------
// Iterator related spec functions.

// ES6 7.4.1 GetIterator(obj, method)
function GetIterator(obj, method) {
  if (IS_UNDEFINED(method)) {
    method = obj[iteratorSymbol];
  }
  if (!IS_CALLABLE(method)) {
    throw %make_type_error(kNotIterable, obj);
  }
  var iterator = %_Call(method, obj);
  if (!IS_RECEIVER(iterator)) {
    throw %make_type_error(kNotAnIterator, iterator);
  }
  return iterator;
}

// ----------------------------------------------------------------------------
// Exports

utils.Export(function(to) {
  to.GetIterator = GetIterator;
  to.GetMethod = GetMethod;
  to.ObjectHasOwnProperty = GlobalObject.prototype.hasOwnProperty;
});

%InstallToContext([
  "object_value_of", ObjectValueOf,
]);

})
