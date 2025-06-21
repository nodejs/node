// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var global = this;

var funs = {
  Object:   [ Object ],
  Function: [ Function ],
  Array:    [ Array ],
  String:   [ String ],
  Boolean:  [ Boolean ],
  Number:   [ Number ],
  Date:     [ Date ],
  RegExp:   [ RegExp ],
  Error:    [ Error, TypeError, RangeError, SyntaxError, ReferenceError,
              EvalError, URIError ]
};
for (var f in funs) {
  for (var i in funs[f]) {

    assertEquals("[object " + f + "]",
      Object.prototype.toString.call(new funs[f][i]),
      funs[f][i]);
    assertEquals("[object Function]",
      Object.prototype.toString.call(funs[f][i]),
      funs[f][i]);
  }
}

function testToStringTag(className) {
  // Using builtin toStringTags
  var obj = {};
  obj[Symbol.toStringTag] = className;
  assertEquals("[object " + className + "]",
               Object.prototype.toString.call(obj));

  // Getter throws
  obj = {};
  Object.defineProperty(obj, Symbol.toStringTag, {
    get: function() { throw className; }
  });
  assertThrowsEquals(function() {
    Object.prototype.toString.call(obj);
  }, className);

  // Getter does not throw
  obj = {};
  Object.defineProperty(obj, Symbol.toStringTag, {
    get: function() { return className; }
  });
  assertEquals("[object " + className + "]",
               Object.prototype.toString.call(obj));

  // Custom, non-builtin toStringTags
  obj = {};
  obj[Symbol.toStringTag] = "X" + className;
  assertEquals("[object X" + className + "]",
               Object.prototype.toString.call(obj));

  // With getter
  obj = {};
  Object.defineProperty(obj, Symbol.toStringTag, {
    get: function() { return "X" + className; }
  });
  assertEquals("[object X" + className + "]",
               Object.prototype.toString.call(obj));

  // Undefined toStringTag should return [object className]
  var obj = className === "Arguments" ?
      (function() { return arguments; })() : new global[className];
  obj[Symbol.toStringTag] = undefined;
  assertEquals("[object " + className + "]",
               Object.prototype.toString.call(obj));

  // With getter
  var obj = className === "Arguments" ?
      (function() { return arguments; })() : new global[className];
  Object.defineProperty(obj, Symbol.toStringTag, {
    get: function() { return undefined; }
  });
  assertEquals("[object " + className + "]",
               Object.prototype.toString.call(obj));
}

[
  "Arguments",
  "Array",
  "Boolean",
  "Date",
  "Error",
  "Function",
  "Number",
  "RegExp",
  "String"
].forEach(testToStringTag);

function testToStringTagNonString(value) {
  var obj = {};
  obj[Symbol.toStringTag] = value;
  assertEquals("[object Object]", Object.prototype.toString.call(obj));

  // With getter
  obj = {};
  Object.defineProperty(obj, Symbol.toStringTag, {
    get: function() { return value; }
  });
  assertEquals("[object Object]", Object.prototype.toString.call(obj));
}

[
  null,
  function() {},
  [],
  {},
  /regexp/,
  42,
  Symbol("sym"),
  new Date(),
  (function() { return arguments; })(),
  true,
  new Error("oops"),
  new String("str")
].forEach(testToStringTagNonString);

function testObjectToStringPropertyDesc() {
  var desc = Object.getOwnPropertyDescriptor(Object.prototype, "toString");
  assertTrue(desc.writable);
  assertFalse(desc.enumerable);
  assertTrue(desc.configurable);
}
testObjectToStringPropertyDesc();

function testObjectToStringOnNonStringValue(obj) {
  Object.defineProperty(obj, Symbol.toStringTag, { value: 1 });
  assertEquals("[object Object]", ({}).toString.call(obj));
}
testObjectToStringOnNonStringValue({});


// Proxies

function assertTag(tag, obj) {
  assertEquals("[object " + tag + "]", Object.prototype.toString.call(obj));
}

assertTag("Object", new Proxy({}, {}));
assertTag("Array", new Proxy([], {}));
assertTag("Function", new Proxy(() => 42, {}));
assertTag("Foo", new Proxy(() => 42, {get() {return "Foo"}}));
assertTag("Function", new Proxy(() => 42, {get() {return 666}}));

var revocable = Proxy.revocable([], {});
revocable.revoke();
assertThrows(() => Object.prototype.toString.call(revocable.proxy), TypeError);

var handler = {};
revocable = Proxy.revocable([], handler);
// The first get() call, i.e., toString() revokes the proxy
handler.get = () => revocable.revoke();
assertEquals("[object Array]", Object.prototype.toString.call(revocable.proxy));
assertThrows(() => Object.prototype.toString.call(revocable.proxy), TypeError);

revocable = Proxy.revocable([], handler);
handler.get = () => {revocable.revoke(); return "value";};
assertEquals("[object value]", Object.prototype.toString.call(revocable.proxy));
assertThrows(() => Object.prototype.toString.call(revocable.proxy), TypeError);


revocable = Proxy.revocable(function() {}, handler);
handler.get = () => revocable.revoke();
assertEquals("[object Function]", Object.prototype.toString.call(revocable.proxy));
assertThrows(() => Object.prototype.toString.call(revocable.proxy), TypeError);

function* gen() { yield 1; }

assertTag("GeneratorFunction", gen);
Object.defineProperty(gen, Symbol.toStringTag, {writable: true});
gen[Symbol.toStringTag] = "different string";
assertTag("different string", gen);
gen[Symbol.toStringTag] = 1;
assertTag("Function", gen);

function overwriteToStringTagWithNonStringValue(tag, obj) {
  assertTag(tag, obj);

  Object.defineProperty(obj, Symbol.toStringTag, {
    configurable: true,
    value: "different string"
  });
  assertTag("different string", obj);

  testObjectToStringOnNonStringValue(obj);
}

overwriteToStringTagWithNonStringValue("global", global);
overwriteToStringTagWithNonStringValue("Generator", gen());

var arrayBuffer = new ArrayBuffer();
overwriteToStringTagWithNonStringValue("ArrayBuffer", arrayBuffer);
overwriteToStringTagWithNonStringValue("DataView", new DataView(arrayBuffer));

overwriteToStringTagWithNonStringValue("Int8Array", new Int8Array());
overwriteToStringTagWithNonStringValue("Uint8Array", new Uint8Array());
overwriteToStringTagWithNonStringValue("Uint8ClampedArray",
  new Uint8ClampedArray());
overwriteToStringTagWithNonStringValue("Int16Array", new Int16Array());
overwriteToStringTagWithNonStringValue("Uint16Array", new Uint16Array());
overwriteToStringTagWithNonStringValue("Int32Array", new Int32Array());
overwriteToStringTagWithNonStringValue("Uint32Array", new Uint32Array());
overwriteToStringTagWithNonStringValue("Float32Array", new Float32Array());
overwriteToStringTagWithNonStringValue("Float64Array", new Float64Array());

var set = new Set();
var map = new Map();

overwriteToStringTagWithNonStringValue("Set", set);
overwriteToStringTagWithNonStringValue("Map", map);

overwriteToStringTagWithNonStringValue("Set Iterator", set[Symbol.iterator]());
overwriteToStringTagWithNonStringValue("Map Iterator", map[Symbol.iterator]());

overwriteToStringTagWithNonStringValue("WeakSet", new WeakSet());
overwriteToStringTagWithNonStringValue("WeakMap", new WeakMap());

overwriteToStringTagWithNonStringValue("Promise", new Promise(function() {}));
