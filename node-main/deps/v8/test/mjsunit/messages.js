// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --stack-size=100 --harmony

function test(f, expected, type) {
  try {
    f();
  } catch (e) {
    assertInstanceof(e, type);
    assertEquals(expected, e.message);
    return;
  }
  assertUnreachable("Exception expected");
}

const typedArrayConstructors = [
  Uint8Array,
  Int8Array,
  Uint16Array,
  Int16Array,
  Uint32Array,
  Int32Array,
  Float32Array,
  Float64Array,
  Uint8ClampedArray
];

// === Error ===

// kCyclicProto
test(function() {
  var o = {};
  o.__proto__ = o;
}, "Cyclic __proto__ value", Error);


// === TypeError ===

// kApplyNonFunction
test(function() {
  Reflect.apply(1, []);
}, "Function.prototype.apply was called on 1, which is a number " +
   "and not a function", TypeError);

test(function() {
  var a = [1, 2];
  Object.freeze(a);
  a.splice(1, 1, [1]);
}, "Cannot assign to read only property '1' of object '[object Array]'",
   TypeError);

test(function() {
  var a = [1];
  Object.seal(a);
  a.shift();
}, "Cannot delete property '0' of [object Array]", TypeError);

// kCalledNonCallable
test(function() {
  [].forEach(1);
}, "number 1 is not a function", TypeError);

// kCalledOnNonObject
test(function() {
  Object.defineProperty(1, "x", {});
}, "Object.defineProperty called on non-object", TypeError);

test(function() {
  (function() {}).apply({}, 1);
}, "CreateListFromArrayLike called on non-object", TypeError);

test(function() {
  Reflect.apply(function() {}, {}, 1);
}, "CreateListFromArrayLike called on non-object", TypeError);

test(function() {
  Reflect.construct(function() {}, 1);
}, "CreateListFromArrayLike called on non-object", TypeError);

// kCalledOnNullOrUndefined
test(function() {
  String.prototype.includes.call(null);
}, "String.prototype.includes called on null or undefined", TypeError);

test(function() {
  String.prototype.match.call(null);
}, "String.prototype.match called on null or undefined", TypeError);

test(function() {
  String.prototype.search.call(null);
}, "String.prototype.search called on null or undefined", TypeError);

test(function() {
  Array.prototype.shift.call(null);
}, "Cannot convert undefined or null to object", TypeError);

test(function() {
  String.prototype.trim.call(null);
}, "String.prototype.trim called on null or undefined", TypeError);

test(function() {
  String.prototype.trimLeft.call(null);
}, "String.prototype.trimLeft called on null or undefined", TypeError);

test(function() {
  String.prototype.trimRight.call(null);
}, "String.prototype.trimRight called on null or undefined", TypeError);

// kCannotFreezeArrayBufferView
test(function() {
  Object.freeze(new Uint16Array(1));
}, "Cannot freeze array buffer views with elements", TypeError);

// kConstAssign
test(function() {
  "use strict";
  const a = 1;
  a = 2;
}, "Assignment to constant variable.", TypeError);

// kCannotConvertToPrimitive
test(function() {
  var o = { toString: function() { return this } };
  [].join(o);
}, "Cannot convert object to primitive value", TypeError);

// kConstructorNotFunction
test(function() {
  Map();
}, "Constructor Map requires 'new'", TypeError);

test(function() {
  Set();
}, "Constructor Set requires 'new'", TypeError);

test(function() {
  Uint16Array(1);
}, "Constructor Uint16Array requires 'new'", TypeError);

test(function() {
  WeakSet();
}, "Constructor WeakSet requires 'new'", TypeError);

test(function() {
  WeakMap();
}, "Constructor WeakMap requires 'new'", TypeError);

// kDataViewNotArrayBuffer
test(function() {
  new DataView(1);
}, "First argument to DataView constructor must be an ArrayBuffer", TypeError);

// kDefineDisallowed
test(function() {
  "use strict";
  var o = {};
  Object.preventExtensions(o);
  Object.defineProperty(o, "x", { value: 1 });
}, "Cannot define property x, object is not extensible", TypeError);

// kDetachedOperation
for (constructor of typedArrayConstructors) {
  test(() => {
    const ta = new constructor([1]);
    %ArrayBufferDetach(ta.buffer);
    ta.find(() => {});
  }, "Cannot perform %TypedArray%.prototype.find on a detached ArrayBuffer", TypeError);

  test(() => {
    const ta = new constructor([1]);
    %ArrayBufferDetach(ta.buffer);
    ta.findIndex(() => {});
  }, "Cannot perform %TypedArray%.prototype.findIndex on a detached ArrayBuffer", TypeError);
}

// kFirstArgumentNotRegExp
test(function() {
  "a".startsWith(/a/);
}, "First argument to String.prototype.startsWith " +
   "must not be a regular expression", TypeError);

test(function() {
  "a".includes(/a/);
}, "First argument to String.prototype.includes " +
   "must not be a regular expression", TypeError);

// kFlagsGetterNonObject
test(function() {
  Object.getOwnPropertyDescriptor(RegExp.prototype, "flags").get.call(1);
}, "RegExp.prototype.flags getter called on non-object 1", TypeError);

// kFunctionBind
test(function() {
  Function.prototype.bind.call(1);
}, "Bind must be called on a function", TypeError);

// kGeneratorRunning
test(function() {
  var iter;
  function* generator() { yield iter.next(); }
  var iter = generator();
  iter.next();
}, "Generator is already running", TypeError);

// kIncompatibleMethodReceiver
test(function() {
  Set.prototype.add.call([]);
}, "Method Set.prototype.add called on incompatible receiver [object Array]",
TypeError);

test(function() {
  WeakSet.prototype.add.call([]);
}, "Method WeakSet.prototype.add called on incompatible receiver [object Array]",
TypeError);

test(function() {
  WeakSet.prototype.delete.call([]);
}, "Method WeakSet.prototype.delete called on incompatible receiver [object Array]",
TypeError);

test(function() {
  WeakMap.prototype.set.call([]);
}, "Method WeakMap.prototype.set called on incompatible receiver [object Array]",
TypeError);

test(function() {
  WeakMap.prototype.delete.call([]);
}, "Method WeakMap.prototype.delete called on incompatible receiver [object Array]",
TypeError);

// kNonCallableInInstanceOfCheck
test(function() {
  1 instanceof {};
}, "Right-hand side of 'instanceof' is not callable", TypeError);

// kNonObjectInInstanceOfCheck
test(function() {
  1 instanceof 1;
}, "Right-hand side of 'instanceof' is not an object", TypeError);

// kInstanceofNonobjectProto
test(function() {
  function f() {}
  var o = new f();
  f.prototype = 1;
  o instanceof f;
}, "Function has non-object prototype '1' in instanceof check", TypeError);

// kInvalidInOperatorUse
test(function() {
  1 in 1;
}, "Cannot use 'in' operator to search for '1' in 1", TypeError);

// kInvalidWeakMapKey
test(function() {
  new WeakMap([[1, 1]]);
}, "Invalid value used as weak map key", TypeError);

test(function() {
  new WeakMap().set(1, 1);
}, "Invalid value used as weak map key", TypeError);

// kInvalidWeakSetValue
test(function() {
  new WeakSet([1]);
}, "Invalid value used in weak set", TypeError);

test(function() {
  new WeakSet().add(1);
}, "Invalid value used in weak set", TypeError);

// kIteratorResultNotAnObject
test(function() {
  var obj = {};
  obj[Symbol.iterator] = function() { return { next: function() { return 1 }}};
  Array.from(obj);
}, "Iterator result 1 is not an object", TypeError);

// kIteratorValueNotAnObject
test(function() {
  new Map([1]);
}, "Iterator value 1 is not an entry object", TypeError);

test(function() {
  let holeyDoubleArray = [, 123.123];
  assertTrue(%HasDoubleElements(holeyDoubleArray));
  assertTrue(%HasHoleyElements(holeyDoubleArray));
  new Map(holeyDoubleArray);
}, "Iterator value undefined is not an entry object", TypeError);

test(function() {
  let holeyDoubleArray = [, 123.123];
  assertTrue(%HasDoubleElements(holeyDoubleArray));
  assertTrue(%HasHoleyElements(holeyDoubleArray));
  new WeakMap(holeyDoubleArray);
}, "Iterator value undefined is not an entry object", TypeError);

// kNotConstructor
test(function() {
  new Symbol();
}, "Symbol is not a constructor", TypeError);

// kNotDateObject
test(function() {
  Date.prototype.getHours.call(1);
}, "this is not a Date object.", TypeError);

// kNotGeneric
test(() => String.prototype.toString.call(1),
    "String.prototype.toString requires that 'this' be a String",
    TypeError);

test(() => String.prototype.valueOf.call(1),
    "String.prototype.valueOf requires that 'this' be a String",
    TypeError);

test(() => Boolean.prototype.toString.call(1),
    "Boolean.prototype.toString requires that 'this' be a Boolean",
    TypeError);

test(() => Boolean.prototype.valueOf.call(1),
    "Boolean.prototype.valueOf requires that 'this' be a Boolean",
    TypeError);

test(() => Number.prototype.toString.call({}),
    "Number.prototype.toString requires that 'this' be a Number",
    TypeError);

test(() => Number.prototype.valueOf.call({}),
    "Number.prototype.valueOf requires that 'this' be a Number",
    TypeError);

test(() => Function.prototype.toString.call(1),
    "Function.prototype.toString requires that 'this' be a Function",
    TypeError);

// kNotTypedArray
test(function() {
  Uint16Array.prototype.forEach.call(1);
}, "this is not a typed array.", TypeError);

// kObjectGetterExpectingFunction
test(function() {
  ({}).__defineGetter__("x", 0);
}, "Object.prototype.__defineGetter__: Expecting function", TypeError);

// kObjectGetterCallable
test(function() {
  Object.defineProperty({}, "x", { get: 1 });
}, "Getter must be a function: 1", TypeError);

// kObjectNotExtensible
test(function() {
  "use strict";
  var o = {};
  Object.freeze(o);
  o.a = 1;
}, "Cannot add property a, object is not extensible", TypeError);

// kObjectSetterExpectingFunction
test(function() {
  ({}).__defineSetter__("x", 0);
}, "Object.prototype.__defineSetter__: Expecting function", TypeError);

// kObjectSetterCallable
test(function() {
  Object.defineProperty({}, "x", { set: 1 });
}, "Setter must be a function: 1", TypeError);

// kPropertyDescObject
test(function() {
  Object.defineProperty({}, "x", 1);
}, "Property description must be an object: 1", TypeError);

// kPropertyNotFunction
test(function() {
  Map.prototype.set = 0;
  new Map([[1, 2]]);
}, "'0' returned for property 'set' of object '#<Map>' is not a function", TypeError);

test(function() {
  Set.prototype.add = 0;
  new Set([1]);
}, "'0' returned for property 'add' of object '#<Set>' is not a function", TypeError);

test(function() {
  WeakMap.prototype.set = 0;
  new WeakMap([[{}, 1]]);
}, "'0' returned for property 'set' of object '#<WeakMap>' is not a function", TypeError);

test(function() {
  WeakSet.prototype.add = 0;
  new WeakSet([{}]);
}, "'0' returned for property 'add' of object '#<WeakSet>' is not a function", TypeError);

// kProtoObjectOrNull
test(function() {
  Object.setPrototypeOf({}, 1);
}, "Object prototype may only be an Object or null: 1", TypeError);

// kRedefineDisallowed
test(function() {
  "use strict";
  var o = {};
  Object.defineProperty(o, "x", { value: 1, configurable: false });
  Object.defineProperty(o, "x", { value: 2 });
}, "Cannot redefine property: x", TypeError);

// kReduceNoInitial
test(function() {
  [].reduce(function() {});
}, "Reduce of empty array with no initial value", TypeError);

// kResolverNotAFunction
test(function() {
  new Promise(1);
}, "Promise resolver 1 is not a function", TypeError);

// kStrictDeleteProperty
test(function() {
  "use strict";
  var o = {};
  Object.defineProperty(o, "p", { value: 1, writable: false });
  delete o.p;
}, "Cannot delete property 'p' of #<Object>", TypeError);

// kStrictPoisonPill
test(function() {
  "use strict";
  arguments.callee;
}, "'caller', 'callee', and 'arguments' properties may not be accessed on " +
   "strict mode functions or the arguments objects for calls to them",
   TypeError);

// kStrictReadOnlyProperty
test(function() {
  "use strict";
  (1).a = 1;
}, "Cannot create property 'a' on number '1'", TypeError);

// kSymbolToString
test(function() {
  "" + Symbol();
}, "Cannot convert a Symbol value to a string", TypeError);

// kSymbolToNumber
test(function() {
  1 + Symbol();
}, "Cannot convert a Symbol value to a number", TypeError);

// kUndefinedOrNullToObject
test(function() {
  Array.prototype.toString.call(null);
}, "Cannot convert undefined or null to object", TypeError);

// kValueAndAccessor
test(function() {
  Object.defineProperty({}, "x", { get: function(){}, value: 1});
}, "Invalid property descriptor. Cannot both specify accessors " +
   "and a value or writable attribute, #<Object>", TypeError);


// === SyntaxError ===

// kInvalidRegExpFlags
test(function() {
  eval("/a/x.test(\"a\");");
}, "Invalid regular expression flags", SyntaxError);

// kInvalidOrUnexpectedToken
test(function() {
  eval("'\n'");
}, "Invalid or unexpected token", SyntaxError);

// kJsonParseUnterminatedString
test(function() {
  JSON.parse('{"a" : "}')
}, "Unterminated string in JSON at position 9 (line 1 column 10)", SyntaxError);

// kJsonParseExpectedPropNameOrRBrace
test(function() {
  JSON.parse("{")
}, "Expected property name or '}' in JSON at position 1 (line 1 column 2)",
  SyntaxError);

// kJsonParseExpectedDoubleQuotedPropertyName
test(function() {
  JSON.parse('{"foo" : 1, }');
}, "Expected double-quoted property name in JSON at position 12 (line 1 column 13)",
  SyntaxError);

// kJsonParseExpectedCommaNameOrRBrack
test(function() {
  JSON.parse("{'foo': 1}");
}, "Expected property name or '}' in JSON at position 1 (line 1 column 2)",
  SyntaxError);

// kJsonParseExpectedCommaNameOrRBrace
test(function() {
  JSON.parse('[1, 2, 3, 4');
}, "Expected ',' or ']' after array element in JSON " +
"at position 11 (line 1 column 12)", SyntaxError);

test(function() {
  JSON.parse('[1, 2, 3, 4g');
}, "Expected ',' or ']' after array element in JSON " +
"at position 11 (line 1 column 12)", SyntaxError);

// kJsonParseExponentPartMissingNumber
test(function() {
  JSON.parse('[1e]');
}, "Exponent part is missing a number in JSON at position 3 (line 1 column 4)",
  SyntaxError);

// kJsonParseExpectedColonAfterPropertyName
test(function() {
  JSON.parse('{"a"}');
}, "Expected ':' after property name in JSON at position 4 (line 1 column 5)",
  SyntaxError);

// kJsonParseUnterminatedFractionNumber
test(function() {
  JSON.parse('{"a": 0.bs}');
}, "Unterminated fractional number in JSON at position 8 (line 1 column 9)",
  SyntaxError);

// kJsonParseUnexpectedNonWhiteSpaceCharacter
test(function() {
  JSON.parse('{"a": 3}a');
}, "Unexpected non-whitespace character after JSON " +
"at position 8 (line 1 column 9)", SyntaxError);

// kJsonParseBadEscapedCharacter
test(function() {
  JSON.parse('{"b" : "\\a"}');
}, "Bad escaped character in JSON at position 9 (line 1 column 10)",
  SyntaxError);

// kJsonParseBadControlCharacter
test(function() {
  JSON.parse('"a\bz"');
}, "Bad control character in string literal in JSON " +
"at position 2 (line 1 column 3)", SyntaxError);

// kJsonParseBadUnicodeEscape
test(function() {
  JSON.parse("[\"\\t\\u");
}, "Bad Unicode escape in JSON at position 6 (line 1 column 7)", SyntaxError);

// kJsonParseNoNumberAfterMinusSign
test(function() {
  JSON.parse('-');
}, "No number after minus sign in JSON at position 1 (line 1 column 2)",
  SyntaxError);

// kJsonParseUnexpectedTokenShortString
test(function () {
  JSON.parse(NaN)
}, "\"NaN\" is not valid JSON", SyntaxError);

// kJsonParseUnexpectedTokenShortString
test(function() {
  JSON.parse("/")
}, "Unexpected token '/', \"/\" is not valid JSON", SyntaxError);

// kJsonParseUnexpectedTokenShortString
test(function () {
  JSON.parse(undefined)
}, "\"undefined\" is not valid JSON", SyntaxError);

// kJsonParseUnexpectedTokenShortString
test(function () {
  JSON.parse(Infinity)
}, "\"Infinity\" is not valid JSON", SyntaxError);

// kJsonParseUnexpectedTokenShortString
test(function () {
  JSON.parse('Bad string')
}, "Unexpected token 'B', \"Bad string\" is not valid JSON", SyntaxError);

// kJsonParseUnexpectedTokenShortString
test(function () {
  JSON.parse({})
}, "\"[object Object]\" is not valid JSON", SyntaxError);

// kJsonParseExpectedPropNameOrRBrace
test(function() {
  JSON.parse("{ 1")
}, "Expected property name or '}' in JSON at position 2 (line 1 column 3)",
  SyntaxError);

// kJsonParseUnexpectedNonWhiteSpaceCharacter
test(function() {
  JSON.parse('"""')
}, "Unexpected non-whitespace character after JSON " +
"at position 2 (line 1 column 3)", SyntaxError);

// kJsonParseUnexpectedTokenStringShortString
test(function() {
  JSON.parse('[1, 2,]');
}, "Unexpected token ']', \"[1, 2,]\" is not valid JSON", SyntaxError);

// kJsonParseUnexpectedTokenStringShortString
test(function() {
  JSON.parse('[1, 2, 3, 4, ]');
}, "Unexpected token ']', \"[1, 2, 3, 4, ]\" is not valid JSON", SyntaxError);

// kJsonParseUnexpectedTokenStringSurroundWithContext
test(function() {
  JSON.parse('[1, 2, 3, 4, 5, , 7, 8, 9, 10]');
}, "Unexpected token ',', ...\" 3, 4, 5, , 7, 8, 9,\"... is not valid JSON", SyntaxError);

// kJsonParseUnexpectedTokenStringStartWithContext
test(function() {
  JSON.parse('[, 2, 3, 4, 5, 6, 7, 8, 9, 10]');
}, "Unexpected token ',', \"[, 2, 3, 4,\"... is not valid JSON", SyntaxError);

// kJsonParseUnexpectedTokenStringEndWithContext
test(function() {
  JSON.parse('[1, 2, 3, 4, 5, 6, 7, ]');
}, "Unexpected token ']', ...\" 5, 6, 7, ]\" is not valid JSON", SyntaxError);

// kMalformedRegExp
test(function() {
  new Function('/(/.test("a");');
}, "Invalid regular expression: /(/: Unterminated group", SyntaxError);

// kParenthesisInArgString
test(function() {
  new Function(")", "");
}, "Arg string terminates parameters early", SyntaxError);

// === ReferenceError ===

// kNotDefined
test(function() {
  "use strict";
  o;
}, "o is not defined", ReferenceError);

// === RangeError ===

// kInvalidOffset
test(function() {
  new Uint8Array(new ArrayBuffer(1),2);
}, "Start offset 2 is outside the bounds of the buffer", RangeError);

// kArrayLengthOutOfRange
test(function() {
  "use strict";
  Object.defineProperty([], "length", { value: 1E100 });
}, "Invalid array length", RangeError);

// kInvalidArrayBufferLength
test(function() {
  new ArrayBuffer(-1);
}, "Invalid array buffer length", RangeError);

// kInvalidArrayLength
test(function() {
  [].length = -1;
}, "Invalid array length", RangeError);

// kInvalidCodePoint
test(function() {
  String.fromCodePoint(-1);
}, "Invalid code point -1", RangeError);

// kInvalidCountValue
test(function() {
  "a".repeat(-1);
}, "Invalid count value: -1", RangeError);

// kInvalidCountValue
test(function() {
  "a".repeat(Infinity);
}, "Invalid count value: Infinity", RangeError);

// kInvalidCountValue
test(function() {
  "a".repeat(-Infinity);
}, "Invalid count value: -Infinity", RangeError);

// kInvalidArrayBufferLength
test(function() {
  new Uint16Array(-1);
}, "Invalid typed array length: -1", RangeError);

// kThrowInvalidStringLength
test(function() {
  "a".padEnd(1 << 30);
}, "Invalid string length", RangeError);

test(function() {
  "a".padStart(1 << 30);
}, "Invalid string length", RangeError);

test(function() {
  "a".repeat(1 << 30);
}, "Invalid string length", RangeError);

test(function() {
  new Array(1 << 30).join();
}, "Invalid string length", RangeError);

// kNormalizationForm
test(function() {
  "".normalize("ABC");
}, "The normalization form should be one of NFC, NFD, NFKC, NFKD.", RangeError);

// kNumberFormatRange
test(function() {
  Number(1).toFixed(101);
}, "toFixed() digits argument must be between 0 and 100", RangeError);

test(function() {
  Number(1).toExponential(101);
}, "toExponential() argument must be between 0 and 100", RangeError);

// kStackOverflow
test(function() {
  function f() { f(Array(1000)); }
  f();
}, "Maximum call stack size exceeded", RangeError);

// kToPrecisionFormatRange
test(function() {
  Number(1).toPrecision(101);
}, "toPrecision() argument must be between 1 and 100", RangeError);

// kToPrecisionFormatRange
test(function() {
  Number(1).toString(100);
}, "toString() radix argument must be between 2 and 36", RangeError);


// === URIError ===

// kURIMalformed
test(function() {
  decodeURI("%%");
}, "URI malformed", URIError);
