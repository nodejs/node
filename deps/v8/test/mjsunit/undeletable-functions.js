// Copyright 2009 the V8 project authors. All rights reserved.
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

// Test that we match ECMAScript in making most builtin functions
// deletable and only specific ones undeletable or read-only.

var array;

array = [
  "toString", "toLocaleString", "join", "pop", "push", "concat", "reverse",
  "shift", "unshift", "slice", "splice", "sort", "filter", "forEach", "some",
  "every", "map", "indexOf", "lastIndexOf", "reduce", "reduceRight"];
CheckEcmaSemantics(Array.prototype, array, "Array prototype");

var old_Array_prototype = Array.prototype;
var new_Array_prototype = {};
for (var i = 0; i < 7; i++) {
  Array.prototype = new_Array_prototype;
  assertEquals(old_Array_prototype, Array.prototype);
}

array = [
  "toString", "toDateString", "toTimeString", "toLocaleString",
  "toLocaleDateString", "toLocaleTimeString", "valueOf", "getTime",
  "getFullYear", "getUTCFullYear", "getMonth", "getUTCMonth", "getDate",
  "getUTCDate", "getDay", "getUTCDay", "getHours", "getUTCHours", "getMinutes",
  "getUTCMinutes", "getSeconds", "getUTCSeconds", "getMilliseconds",
  "getUTCMilliseconds", "getTimezoneOffset", "setTime", "setMilliseconds",
  "setUTCMilliseconds", "setSeconds", "setUTCSeconds", "setMinutes",
  "setUTCMinutes", "setHours", "setUTCHours", "setDate", "setUTCDate",
  "setMonth", "setUTCMonth", "setFullYear", "setUTCFullYear", "toGMTString",
  "toUTCString", "getYear", "setYear", "toISOString", "toJSON"];
CheckEcmaSemantics(Date.prototype, array, "Date prototype");

array = [
  "random", "abs", "acos", "asin", "atan", "ceil", "cos", "exp", "floor", "log",
  "round", "sin", "sqrt", "tan", "atan2", "pow", "max", "min"];
CheckEcmaSemantics(Math, array, "Math1");

CheckEcmaSemantics(Date, ["UTC", "parse", "now"], "Date");

array = [
  "E", "LN10", "LN2", "LOG2E", "LOG10E", "PI", "SQRT1_2", "SQRT2"];
CheckDontDelete(Math, array, "Math2");

array = [
  "escape", "unescape", "decodeURI", "decodeURIComponent", "encodeURI",
  "encodeURIComponent", "isNaN", "isFinite", "parseInt", "parseFloat", "eval",
  "execScript"];
CheckEcmaSemantics(this, array, "Global");
CheckReadOnlyAttr(this, "Infinity");
CheckReadOnlyAttr(this, "NaN");
CheckReadOnlyAttr(this, "undefined");

array = ["exec", "test", "toString", "compile"];
CheckEcmaSemantics(RegExp.prototype, array, "RegExp prototype");

array = [
  "toString", "toLocaleString", "valueOf", "hasOwnProperty",
  "isPrototypeOf", "propertyIsEnumerable", "__defineGetter__",
  "__lookupGetter__", "__defineSetter__", "__lookupSetter__"];
CheckEcmaSemantics(Object.prototype, array, "Object prototype");

var old_Object_prototype = Object.prototype;
var new_Object_prototype = {};
for (var i = 0; i < 7; i++) {
  Object.prototype = new_Object_prototype;
  assertEquals(old_Object_prototype, Object.prototype);
}

array = [
  "toString", "valueOf", "toJSON"];
CheckEcmaSemantics(Boolean.prototype, array, "Boolean prototype");

array = [
  "toString", "toLocaleString", "valueOf", "toFixed", "toExponential",
  "toPrecision", "toJSON"];
CheckEcmaSemantics(Number.prototype, array, "Number prototype");

CheckEcmaSemantics(Function.prototype, ["toString"], "Function prototype");
CheckEcmaSemantics(Date.prototype, ["constructor"], "Date prototype constructor");

array = [
  "charAt", "charCodeAt", "concat", "indexOf",
  "lastIndexOf", "localeCompare", "match", "replace", "search", "slice",
  "split", "substring", "substr", "toLowerCase", "toLocaleLowerCase",
  "toUpperCase", "toLocaleUpperCase", "link", "anchor", "fontcolor", "fontsize",
  "big", "blink", "bold", "fixed", "italics", "small", "strike", "sub", "sup",
  "toJSON", "toString", "valueOf"];
CheckEcmaSemantics(String.prototype, array, "String prototype");
CheckEcmaSemantics(String, ["fromCharCode"], "String");


function CheckEcmaSemantics(type, props, name) {
  print(name);
  for (var i = 0; i < props.length; i++) {
    CheckDeletable(type, props[i]);
  }
}


function CheckDontDelete(type, props, name) {
  print(name);
  for (var i = 0; i < props.length; i++) {
    CheckDontDeleteAttr(type, props[i]);
  }
}


function CheckDeletable(type, prop) {
  var old = type[prop];
  var hasOwnProperty = Object.prototype.hasOwnProperty;
  if (!type[prop]) return;
  assertTrue(type.hasOwnProperty(prop), "inherited: " + prop);
  var deleted = delete type[prop];
  assertTrue(deleted, "delete operator returned false: " + prop);
  assertFalse(hasOwnProperty.call(type, prop), "still there after delete: " + prop);
  type[prop] = "foo";
  assertEquals("foo", type[prop], "not overwritable: " + prop);
  type[prop] = old;
}


function CheckDontDeleteAttr(type, prop) {
  var old = type[prop];
  if (!type[prop]) return;
  assertTrue(type.hasOwnProperty(prop), "inherited: " + prop);
  var deleted = delete type[prop];
  assertFalse(deleted, "delete operator returned true: " + prop);
  assertTrue(type.hasOwnProperty(prop), "not there after delete: " + prop);
  type[prop] = "foo";
  assertFalse("foo" == type[prop], "overwritable: " + prop);
}


function CheckReadOnlyAttr(type, prop) {
  var old = type[prop];
  if (!type[prop]) return;
  assertTrue(type.hasOwnProperty(prop), "inherited: " + prop);
  var deleted = delete type[prop];
  assertFalse(deleted, "delete operator returned true: " + prop);
  assertTrue(type.hasOwnProperty(prop), "not there after delete: " + prop);
  type[prop] = "foo";
  assertEquals(old, type[prop], "overwritable: " + prop);
}

print("OK");
