// Copyright 2006-2008 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// This file relies on the fact that the following declarations have been made
//
// in runtime.js:
// const $Object = global.Object;
// const $Boolean = global.Boolean;
// const $Number = global.Number;
// const $Function = global.Function;
// const $Array = global.Array;
// const $NaN = 0/0;
//
// in math.js:
// const $floor = MathFloor

const $isNaN = GlobalIsNaN;
const $isFinite = GlobalIsFinite;

// ----------------------------------------------------------------------------


// Helper function used to install functions on objects.
function InstallFunctions(object, attributes, functions) {
  for (var i = 0; i < functions.length; i += 2) {
    var key = functions[i];
    var f = functions[i + 1];
    %FunctionSetName(f, key);
    %SetProperty(object, key, f, attributes);
  }
}

// Emulates JSC by installing functions on a hidden prototype that
// lies above the current object/prototype.  This lets you override
// functions on String.prototype etc. and then restore the old function
// with delete.  See http://code.google.com/p/chromium/issues/detail?id=1717
function InstallFunctionsOnHiddenPrototype(object, attributes, functions) {
  var hidden_prototype = new $Object();
  %SetHiddenPrototype(object, hidden_prototype);
  InstallFunctions(hidden_prototype, attributes, functions);
}


// ----------------------------------------------------------------------------


// ECMA 262 - 15.1.4
function GlobalIsNaN(number) {
  var n = ToNumber(number);
  return NUMBER_IS_NAN(n);
}


// ECMA 262 - 15.1.5
function GlobalIsFinite(number) {
  return %NumberIsFinite(ToNumber(number));
}


// ECMA-262 - 15.1.2.2
function GlobalParseInt(string, radix) {
  if (radix === void 0) {
    // Some people use parseInt instead of Math.floor.  This
    // optimization makes parseInt on a Smi 12 times faster (60ns
    // vs 800ns).  The following optimization makes parseInt on a
    // non-Smi number 9 times faster (230ns vs 2070ns).  Together
    // they make parseInt on a string 1.4% slower (274ns vs 270ns).
    if (%_IsSmi(string)) return string;
    if (IS_NUMBER(string) &&
        ((string < -0.01 && -1e9 < string) ||
            (0.01 < string && string < 1e9))) {
      // Truncate number.
      return string | 0;
    }
    radix = 0;
  } else {
    radix = TO_INT32(radix);
    if (!(radix == 0 || (2 <= radix && radix <= 36)))
      return $NaN;
  }
  return %StringParseInt(ToString(string), radix);
}


// ECMA-262 - 15.1.2.3
function GlobalParseFloat(string) {
  return %StringParseFloat(ToString(string));
}


function GlobalEval(x) {
  if (!IS_STRING(x)) return x;

  var global_receiver = %GlobalReceiver(global);
  var this_is_global_receiver = (this === global_receiver);
  var global_is_detached = (global === global_receiver);

  if (!this_is_global_receiver || global_is_detached) {
    throw new $EvalError('The "this" object passed to eval must ' +
                         'be the global object from which eval originated');
  }

  var f = %CompileString(x, false);
  if (!IS_FUNCTION(f)) return f;

  return f.call(this);
}


// execScript for IE compatibility.
function GlobalExecScript(expr, lang) {
  // NOTE: We don't care about the character casing.
  if (!lang || /javascript/i.test(lang)) {
    var f = %CompileString(ToString(expr), false);
    f.call(%GlobalReceiver(global));
  }
  return null;
}


// ----------------------------------------------------------------------------


function SetupGlobal() {
  // ECMA 262 - 15.1.1.1.
  %SetProperty(global, "NaN", $NaN, DONT_ENUM | DONT_DELETE);

  // ECMA-262 - 15.1.1.2.
  %SetProperty(global, "Infinity", 1/0, DONT_ENUM | DONT_DELETE);

  // ECMA-262 - 15.1.1.3.
  %SetProperty(global, "undefined", void 0, DONT_ENUM | DONT_DELETE);

  // Setup non-enumerable function on the global object.
  InstallFunctions(global, DONT_ENUM, $Array(
    "isNaN", GlobalIsNaN,
    "isFinite", GlobalIsFinite,
    "parseInt", GlobalParseInt,
    "parseFloat", GlobalParseFloat,
    "eval", GlobalEval,
    "execScript", GlobalExecScript
  ));
}

SetupGlobal();


// ----------------------------------------------------------------------------
// Boolean (first part of definition)


%SetCode($Boolean, function(x) {
  if (%_IsConstructCall()) {
    %_SetValueOf(this, ToBoolean(x));
  } else {
    return ToBoolean(x);
  }
});

%FunctionSetPrototype($Boolean, new $Boolean(false));

%SetProperty($Boolean.prototype, "constructor", $Boolean, DONT_ENUM);

// ----------------------------------------------------------------------------
// Object

$Object.prototype.constructor = $Object;

// ECMA-262 - 15.2.4.2
function ObjectToString() {
  var c = %_ClassOf(this);
  // Hide Arguments from the outside.
  if (c === 'Arguments') c  = 'Object';
  return "[object " + c + "]";
}


// ECMA-262 - 15.2.4.3
function ObjectToLocaleString() {
  return this.toString();
}


// ECMA-262 - 15.2.4.4
function ObjectValueOf() {
  return this;
}


// ECMA-262 - 15.2.4.5
function ObjectHasOwnProperty(V) {
  return %HasLocalProperty(ToObject(this), ToString(V));
}


// ECMA-262 - 15.2.4.6
function ObjectIsPrototypeOf(V) {
  if (!IS_OBJECT(V) && !IS_FUNCTION(V)) return false;
  return %IsInPrototypeChain(this, V);
}


// ECMA-262 - 15.2.4.6
function ObjectPropertyIsEnumerable(V) {
  if (this == null) return false;
  if (!IS_OBJECT(this) && !IS_FUNCTION(this)) return false;
  return %IsPropertyEnumerable(this, ToString(V));
}


// Extensions for providing property getters and setters.
function ObjectDefineGetter(name, fun) {
  if (this == null) {
    throw new $TypeError('Object.prototype.__defineGetter__: this is Null');
  }
  if (!IS_FUNCTION(fun)) {
    throw new $TypeError('Object.prototype.__defineGetter__: Expecting function');
  }
  return %DefineAccessor(ToObject(this), ToString(name), GETTER, fun);
}


function ObjectLookupGetter(name) {
  if (this == null) {
    throw new $TypeError('Object.prototype.__lookupGetter__: this is Null');
  }
  return %LookupAccessor(ToObject(this), ToString(name), GETTER);
}


function ObjectDefineSetter(name, fun) {
  if (this == null) {
    throw new $TypeError('Object.prototype.__defineSetter__: this is Null');
  }
  if (!IS_FUNCTION(fun)) {
    throw new $TypeError(
        'Object.prototype.__defineSetter__: Expecting function');
  }
  return %DefineAccessor(ToObject(this), ToString(name), SETTER, fun);
}


function ObjectLookupSetter(name) {
  if (this == null) {
    throw new $TypeError('Object.prototype.__lookupSetter__: this is Null');
  }
  return %LookupAccessor(ToObject(this), ToString(name), SETTER);
}


%SetCode($Object, function(x) {
  if (%_IsConstructCall()) {
    if (x == null) return this;
    return ToObject(x);
  } else {
    if (x == null) return { };
    return ToObject(x);
  }
});


// ----------------------------------------------------------------------------


function SetupObject() {
  // Setup non-enumerable functions on the Object.prototype object.
  InstallFunctions($Object.prototype, DONT_ENUM, $Array(
    "toString", ObjectToString,
    "toLocaleString", ObjectToLocaleString,
    "valueOf", ObjectValueOf,
    "hasOwnProperty", ObjectHasOwnProperty,
    "isPrototypeOf", ObjectIsPrototypeOf,
    "propertyIsEnumerable", ObjectPropertyIsEnumerable,
    "__defineGetter__", ObjectDefineGetter,
    "__lookupGetter__", ObjectLookupGetter,
    "__defineSetter__", ObjectDefineSetter,
    "__lookupSetter__", ObjectLookupSetter
  ));
}

SetupObject();


// ----------------------------------------------------------------------------
// Boolean

function BooleanToString() {
  // NOTE: Both Boolean objects and values can enter here as
  // 'this'. This is not as dictated by ECMA-262.
  if (!IS_BOOLEAN(this) && !IS_BOOLEAN_WRAPPER(this))
    throw new $TypeError('Boolean.prototype.toString is not generic');
  return ToString(%_ValueOf(this));
}


function BooleanValueOf() {
  // NOTE: Both Boolean objects and values can enter here as
  // 'this'. This is not as dictated by ECMA-262.
  if (!IS_BOOLEAN(this) && !IS_BOOLEAN_WRAPPER(this))
    throw new $TypeError('Boolean.prototype.valueOf is not generic');
  return %_ValueOf(this);
}


function BooleanToJSON(key) {
  return CheckJSONPrimitive(this.valueOf());
}


// ----------------------------------------------------------------------------


function SetupBoolean() {
  InstallFunctions($Boolean.prototype, DONT_ENUM, $Array(
    "toString", BooleanToString,
    "valueOf", BooleanValueOf,
    "toJSON", BooleanToJSON
  ));
}

SetupBoolean();

// ----------------------------------------------------------------------------
// Number

// Set the Number function and constructor.
%SetCode($Number, function(x) {
  var value = %_ArgumentsLength() == 0 ? 0 : ToNumber(x);
  if (%_IsConstructCall()) {
    %_SetValueOf(this, value);
  } else {
    return value;
  }
});

%FunctionSetPrototype($Number, new $Number(0));

// ECMA-262 section 15.7.4.2.
function NumberToString(radix) {
  // NOTE: Both Number objects and values can enter here as
  // 'this'. This is not as dictated by ECMA-262.
  var number = this;
  if (!IS_NUMBER(this)) {
    if (!IS_NUMBER_WRAPPER(this))
      throw new $TypeError('Number.prototype.toString is not generic');
    // Get the value of this number in case it's an object.
    number = %_ValueOf(this);
  }
  // Fast case: Convert number in radix 10.
  if (IS_UNDEFINED(radix) || radix === 10) {
    return ToString(number);
  }

  // Convert the radix to an integer and check the range.
  radix = TO_INTEGER(radix);
  if (radix < 2 || radix > 36) {
    throw new $RangeError('toString() radix argument must be between 2 and 36');
  }
  // Convert the number to a string in the given radix.
  return %NumberToRadixString(number, radix);
}


// ECMA-262 section 15.7.4.3
function NumberToLocaleString() {
  return this.toString();
}


// ECMA-262 section 15.7.4.4
function NumberValueOf() {
  // NOTE: Both Number objects and values can enter here as
  // 'this'. This is not as dictated by ECMA-262.
  if (!IS_NUMBER(this) && !IS_NUMBER_WRAPPER(this))
    throw new $TypeError('Number.prototype.valueOf is not generic');
  return %_ValueOf(this);
}


// ECMA-262 section 15.7.4.5
function NumberToFixed(fractionDigits) {
  var f = TO_INTEGER(fractionDigits);
  if (f < 0 || f > 20) {
    throw new $RangeError("toFixed() digits argument must be between 0 and 20");
  }
  var x = ToNumber(this);
  return %NumberToFixed(x, f);
}


// ECMA-262 section 15.7.4.6
function NumberToExponential(fractionDigits) {
  var f = -1;
  if (!IS_UNDEFINED(fractionDigits)) {
    f = TO_INTEGER(fractionDigits);
    if (f < 0 || f > 20) {
      throw new $RangeError("toExponential() argument must be between 0 and 20");
    }
  }
  var x = ToNumber(this);
  return %NumberToExponential(x, f);
}


// ECMA-262 section 15.7.4.7
function NumberToPrecision(precision) {
  if (IS_UNDEFINED(precision)) return ToString(%_ValueOf(this));
  var p = TO_INTEGER(precision);
  if (p < 1 || p > 21) {
    throw new $RangeError("toPrecision() argument must be between 1 and 21");
  }
  var x = ToNumber(this);
  return %NumberToPrecision(x, p);
}


function CheckJSONPrimitive(val) {
  if (!IsPrimitive(val))
    throw MakeTypeError('result_not_primitive', ['toJSON', val]);
  return val;
}


function NumberToJSON(key) {
  return CheckJSONPrimitive(this.valueOf());
}


// ----------------------------------------------------------------------------

function SetupNumber() {
  // Setup the constructor property on the Number prototype object.
  %SetProperty($Number.prototype, "constructor", $Number, DONT_ENUM);

  // ECMA-262 section 15.7.3.1.
  %SetProperty($Number,
               "MAX_VALUE",
               1.7976931348623157e+308,
               DONT_ENUM | DONT_DELETE | READ_ONLY);

  // ECMA-262 section 15.7.3.2.
  %SetProperty($Number, "MIN_VALUE", 5e-324, DONT_ENUM | DONT_DELETE | READ_ONLY);

  // ECMA-262 section 15.7.3.3.
  %SetProperty($Number, "NaN", $NaN, DONT_ENUM | DONT_DELETE | READ_ONLY);

  // ECMA-262 section 15.7.3.4.
  %SetProperty($Number,
               "NEGATIVE_INFINITY",
               -1/0,
               DONT_ENUM | DONT_DELETE | READ_ONLY);

  // ECMA-262 section 15.7.3.5.
  %SetProperty($Number,
               "POSITIVE_INFINITY",
               1/0,
               DONT_ENUM | DONT_DELETE | READ_ONLY);

  // Setup non-enumerable functions on the Number prototype object.
  InstallFunctions($Number.prototype, DONT_ENUM, $Array(
    "toString", NumberToString,
    "toLocaleString", NumberToLocaleString,
    "valueOf", NumberValueOf,
    "toFixed", NumberToFixed,
    "toExponential", NumberToExponential,
    "toPrecision", NumberToPrecision,
    "toJSON", NumberToJSON
  ));
}

SetupNumber();



// ----------------------------------------------------------------------------
// Function

$Function.prototype.constructor = $Function;

function FunctionSourceString(func) {
  if (!IS_FUNCTION(func)) {
    throw new $TypeError('Function.prototype.toString is not generic');
  }

  var source = %FunctionGetSourceCode(func);
  if (!IS_STRING(source)) {
    var name = %FunctionGetName(func);
    if (name) {
      // Mimic what KJS does.
      return 'function ' + name + '() { [native code] }';
    } else {
      return 'function () { [native code] }';
    }
  }

  // Censor occurrences of internal calls.  We do that for all
  // functions and don't cache under the assumption that people rarly
  // convert functions to strings.  Note that we (apparently) can't
  // use regular expression literals in natives files.
  var regexp = ORIGINAL_REGEXP("%(\\w+\\()", "gm");
  if (source.match(regexp)) source = source.replace(regexp, "$1");
  var name = %FunctionGetName(func);
  return 'function ' + name + source;
}


function FunctionToString() {
  return FunctionSourceString(this);
}


function NewFunction(arg1) {  // length == 1
  var n = %_ArgumentsLength();
  var p = '';
  if (n > 1) {
    p = new $Array(n - 1);
    // Explicitly convert all parameters to strings.
    // Array.prototype.join replaces null with empty strings which is
    // not appropriate.
    for (var i = 0; i < n - 1; i++) p[i] = ToString(%_Arguments(i));
    p = p.join(',');
    // If the formal parameters string include ) - an illegal
    // character - it may make the combined function expression
    // compile. We avoid this problem by checking for this early on.
    if (p.indexOf(')') != -1) throw MakeSyntaxError('unable_to_parse',[]);
  }
  var body = (n > 0) ? ToString(%_Arguments(n - 1)) : '';
  var source = '(function(' + p + ') {\n' + body + '\n})';

  // The call to SetNewFunctionAttributes will ensure the prototype
  // property of the resulting function is enumerable (ECMA262, 15.3.5.2).
  var f = %CompileString(source, false)();
  %FunctionSetName(f, "anonymous");
  return %SetNewFunctionAttributes(f);
}

%SetCode($Function, NewFunction);

// ----------------------------------------------------------------------------

function SetupFunction() {
  InstallFunctions($Function.prototype, DONT_ENUM, $Array(
    "toString", FunctionToString
  ));
}

SetupFunction();
