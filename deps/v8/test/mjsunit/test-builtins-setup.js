// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


function CheckNoPrototype(object) {
  var desc = Object.getOwnPropertyDescriptor(object, "prototype");
  assertEquals(undefined, desc);
}

function CheckReadonlyPrototype(object) {
  var desc = Object.getOwnPropertyDescriptor(object, "prototype");
  assertTrue(desc != undefined);
  assertFalse(desc.enumerable);
  assertFalse(desc.configurable);
  assertFalse(desc.writable);
}

function CheckMethodEx(object, prop_name, function_name, length) {
  var desc = Object.getOwnPropertyDescriptor(object, prop_name);
  assertTrue(desc != undefined);
  assertEquals(function_name, desc.value.name);
  assertEquals(length, desc.value.length, "Bad length of \"" + function_name + "\"");
  assertFalse(desc.enumerable);
  assertTrue(desc.configurable);
  assertTrue(desc.writable);
  assertThrows(() => new desc.value());
  // Check that built-in function is strict.
  assertThrows(() => desc.value.arguments);
  assertThrows(() => desc.value.caller);
}

function CheckMethod(object, name, length) {
  CheckMethodEx(object, name, name, length);
}

function CheckGetter(object, name) {
  var desc = Object.getOwnPropertyDescriptor(object, name);
  assertTrue(desc != undefined);
  var expected_name = "get ";
  if (typeof(name) == "symbol") {
    name = name.toString().match("Symbol\\((.*)\\)")[1];
    expected_name += "[" + name + "]";
  } else {
    expected_name += name;
  }
  assertEquals(expected_name, desc.get.name);
  assertEquals(0, desc.get.length);
  assertFalse(desc.enumerable);
  assertTrue(desc.configurable);
  // Check that built-in function is strict.
  assertThrows(() => desc.get.arguments);
  assertThrows(() => desc.get.caller);
}


(function TestIntrinsicConstructors() {
  CheckReadonlyPrototype(Object);
  CheckReadonlyPrototype(Function);
  CheckReadonlyPrototype(Number);
  CheckReadonlyPrototype(Boolean);
  CheckReadonlyPrototype(Symbol);
  CheckReadonlyPrototype(Date);
  CheckReadonlyPrototype(RegExp);
  CheckReadonlyPrototype(DataView);
  CheckReadonlyPrototype(ArrayBuffer);

  var AsyncFunction = (async function(){}).constructor;
  CheckReadonlyPrototype(AsyncFunction);
  var GeneratorFunction = (function*(){}).constructor;
  CheckReadonlyPrototype(GeneratorFunction);

  CheckReadonlyPrototype(Error);
  CheckReadonlyPrototype(SyntaxError);
  CheckReadonlyPrototype(RangeError);
  CheckReadonlyPrototype(TypeError);
  CheckReadonlyPrototype(ReferenceError);
  CheckReadonlyPrototype(EvalError);
  CheckReadonlyPrototype(URIError);
  CheckReadonlyPrototype(Error);
})();


(function TestIntl() {
  if (typeof (Intl) == "undefined") return;
  CheckMethod(Intl, "getCanonicalLocales", 1);

  CheckReadonlyPrototype(Intl.Collator);
  CheckMethod(Intl.Collator, "supportedLocalesOf", 1);
  CheckGetter(Intl.Collator.prototype, "compare");
  CheckMethod(Intl.Collator.prototype, "resolvedOptions", 0);

  CheckReadonlyPrototype(Intl.NumberFormat);
  CheckMethod(Intl.NumberFormat, "supportedLocalesOf", 1);
  CheckGetter(Intl.NumberFormat.prototype, "format");
  CheckMethod(Intl.NumberFormat.prototype, "resolvedOptions", 0);

  CheckReadonlyPrototype(Intl.DateTimeFormat);
  CheckMethod(Intl.DateTimeFormat, "supportedLocalesOf", 1);
  CheckGetter(Intl.DateTimeFormat.prototype, "format");
  CheckMethod(Intl.DateTimeFormat.prototype, "resolvedOptions", 0);
  CheckMethod(Intl.DateTimeFormat.prototype, "formatToParts", 1);

  CheckReadonlyPrototype(Intl.v8BreakIterator);
  CheckMethod(Intl.v8BreakIterator, "supportedLocalesOf", 1);
  CheckMethod(Intl.v8BreakIterator.prototype, "resolvedOptions", 0);
  CheckGetter(Intl.v8BreakIterator.prototype, "adoptText");
  CheckGetter(Intl.v8BreakIterator.prototype, "first");
  CheckGetter(Intl.v8BreakIterator.prototype, "next");
  CheckGetter(Intl.v8BreakIterator.prototype, "current");
  CheckGetter(Intl.v8BreakIterator.prototype, "breakType");

  CheckMethod(String.prototype, "localeCompare", 1);
  CheckMethod(String.prototype, "toLocaleLowerCase", 0);
  CheckMethod(String.prototype, "toLocaleUpperCase", 0);

  CheckMethod(Number.prototype, "toLocaleString", 0);

  CheckMethod(Date.prototype, "toLocaleString", 0);
  CheckMethod(Date.prototype, "toLocaleDateString", 0);
  CheckMethod(Date.prototype, "toLocaleTimeString", 0);
})();


(function TestCollection() {
  CheckReadonlyPrototype(Set);
  CheckMethod(Set.prototype, "add", 1);
  CheckMethod(Set.prototype, "delete", 1);
  CheckMethod(Set.prototype, "entries", 0);
  assertTrue(Set.prototype.keys === Set.prototype.values);
  assertTrue(Set.prototype[Symbol.iterator] === Set.prototype.values);
  CheckMethod(Set.prototype, "values", 0);

  var SetIteratorPrototype = (new Set())[Symbol.iterator]().__proto__;
  CheckMethod(SetIteratorPrototype, "next", 0);
  assertEquals("Set Iterator", SetIteratorPrototype[Symbol.toStringTag]);
  assertEquals(
      undefined,
      Object.getOwnPropertyDescriptor(SetIteratorPrototype, "constructor"));

  CheckReadonlyPrototype(Map);
  CheckMethod(Map.prototype, "set", 2);
  CheckMethod(Map.prototype, "delete", 1);
  CheckMethod(Map.prototype, "entries", 0);
  CheckMethod(Map.prototype, "keys", 0);
  CheckMethod(Map.prototype, "values", 0);
  assertTrue(Map.prototype[Symbol.iterator] === Map.prototype.entries);

  var MapIteratorPrototype = (new Map())[Symbol.iterator]().__proto__;
  CheckMethod(MapIteratorPrototype, "next", 0);
  assertEquals("Map Iterator", MapIteratorPrototype[Symbol.toStringTag]);
  assertEquals(
      undefined,
      Object.getOwnPropertyDescriptor(MapIteratorPrototype, "constructor"));

  CheckReadonlyPrototype(WeakSet);
  assertEquals(0, WeakSet.length);
  CheckMethod(WeakSet.prototype, "add", 1);
  CheckMethod(WeakSet.prototype, "delete", 1);
  CheckMethod(WeakSet.prototype, "has", 1);

  CheckReadonlyPrototype(WeakMap);
  assertEquals(0, WeakMap.length);
  CheckMethod(WeakMap.prototype, "delete", 1);
  CheckMethod(WeakMap.prototype, "get", 1);
  CheckMethod(WeakMap.prototype, "has", 1);
  CheckMethod(WeakMap.prototype, "set", 2);
})();


(function TestTypedArrays() {
  var TypedArray = Uint8Array.__proto__;

  CheckReadonlyPrototype(Int8Array);
  CheckReadonlyPrototype(Uint8Array);
  CheckReadonlyPrototype(Uint8ClampedArray);
  CheckReadonlyPrototype(Int16Array);
  CheckReadonlyPrototype(Uint16Array);
  CheckReadonlyPrototype(Int32Array);
  CheckReadonlyPrototype(Uint32Array);
  CheckReadonlyPrototype(Float32Array);
  CheckReadonlyPrototype(Float64Array);

  CheckReadonlyPrototype(TypedArray);
  CheckMethod(TypedArray, "of", 0);
  CheckMethod(TypedArray, "from", 1);

  CheckMethod(TypedArray.prototype, "subarray", 2);
  CheckMethod(TypedArray.prototype, "set", 1);
  CheckGetter(TypedArray.prototype, Symbol.toStringTag);
  CheckMethod(TypedArray.prototype, "filter", 1);
  CheckMethod(TypedArray.prototype, "find", 1);
  CheckMethod(TypedArray.prototype, "findIndex", 1);
  CheckMethod(TypedArray.prototype, "sort", 1);
  CheckMethod(TypedArray.prototype, "toLocaleString", 0);
  CheckMethod(TypedArray.prototype, "join", 1);
})();


(function TestArray() {
  CheckReadonlyPrototype(Array);

  CheckMethod(Array, "of", 0);
  CheckMethod(Array, "from", 1);

  CheckMethod(Array.prototype, "concat", 1);
  CheckMethod(Array.prototype, "copyWithin", 2);
  CheckMethod(Array.prototype, "every", 1);
  CheckMethod(Array.prototype, "fill", 1);
  CheckMethod(Array.prototype, "filter", 1);
  CheckMethod(Array.prototype, "find", 1);
  CheckMethod(Array.prototype, "findIndex", 1);
  CheckMethod(Array.prototype, "includes", 1);
  CheckMethod(Array.prototype, "indexOf", 1);
  CheckMethod(Array.prototype, "join", 1);
  CheckMethod(Array.prototype, "lastIndexOf", 1);
  CheckMethod(Array.prototype, "map", 1);
  CheckMethod(Array.prototype, "pop", 0);
  CheckMethod(Array.prototype, "push", 1);
  CheckMethod(Array.prototype, "reduce", 1);
  CheckMethod(Array.prototype, "reduceRight", 1);
  CheckMethod(Array.prototype, "reverse", 0);
  CheckMethod(Array.prototype, "shift", 0);
  CheckMethod(Array.prototype, "slice", 2);
  CheckMethod(Array.prototype, "some", 1);
  CheckMethod(Array.prototype, "sort", 1);
  CheckMethod(Array.prototype, "splice", 2);
  CheckMethod(Array.prototype, "toLocaleString", 0);
  CheckMethod(Array.prototype, "toString", 0);
  CheckMethod(Array.prototype, "unshift", 1);

  CheckMethod(Array.prototype, "entries", 0);
  CheckMethod(Array.prototype, "forEach", 1);
  CheckMethod(Array.prototype, "keys", 0);
  CheckMethodEx(Array.prototype, Symbol.iterator, "values", 0);
})();


(function TestPromise() {
  CheckReadonlyPrototype(Promise);
  CheckMethod(Promise, "all", 1);
  CheckMethod(Promise, "race", 1);
  CheckMethod(Promise, "reject", 1);
  CheckMethod(Promise, "resolve", 1);
})();


(function TestProxy() {
  CheckNoPrototype(Proxy);
  CheckMethod(Proxy, "revocable", 2);
})();


(function TestString() {
  CheckReadonlyPrototype(String);
  CheckMethod(String, "raw", 1);

  CheckMethod(String.prototype, "codePointAt", 1);
  CheckMethod(String.prototype, "match", 1);
  CheckMethod(String.prototype, "padEnd", 1);
  CheckMethod(String.prototype, "padStart", 1);
  CheckMethod(String.prototype, "repeat", 1);
  CheckMethod(String.prototype, "search", 1);
  CheckMethod(String.prototype, "link", 1);
  CheckMethod(String.prototype, "anchor", 1);
  CheckMethod(String.prototype, "fontcolor", 1);
  CheckMethod(String.prototype, "fontsize", 1);
  CheckMethod(String.prototype, "big", 0);
  CheckMethod(String.prototype, "blink", 0);
  CheckMethod(String.prototype, "bold", 0);
  CheckMethod(String.prototype, "fixed", 0);
  CheckMethod(String.prototype, "italics", 0);
  CheckMethod(String.prototype, "small", 0);
  CheckMethod(String.prototype, "strike", 0);
  CheckMethod(String.prototype, "sub", 0);
  CheckMethod(String.prototype, "sup", 0);
})();
