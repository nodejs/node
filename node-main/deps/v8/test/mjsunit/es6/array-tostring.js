// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var global = this;

var funs = {
  Object:   [ Object ],
  Function: [ Function ],
  String:   [ String ],
  Boolean:  [ Boolean ],
  Number:   [ Number ],
  Date:     [ Date ],
  RegExp:   [ RegExp ],
  Error:    [ Error, TypeError, RangeError, SyntaxError, ReferenceError,
              EvalError, URIError ]
}
for (f in funs) {
  for (i in funs[f]) {
    assertEquals("[object " + f + "]",
                 Array.prototype.toString.call(new funs[f][i]),
                 funs[f][i]);
    assertEquals("[object Function]",
                 Array.prototype.toString.call(funs[f][i]),
                 funs[f][i]);
  }
}


function testToStringTag(className) {
  // Using builtin toStringTags
  var obj = {};
  obj[Symbol.toStringTag] = className;
  assertEquals("[object " + className + "]",
               Array.prototype.toString.call(obj));

  // Getter throws
  obj = {};
  Object.defineProperty(obj, Symbol.toStringTag, {
    get: function() { throw className; }
  });
  assertThrowsEquals(function() {
    Array.prototype.toString.call(obj);
  }, className);

  // Getter does not throw
  obj = {};
  Object.defineProperty(obj, Symbol.toStringTag, {
    get: function() { return className; }
  });
  assertEquals("[object " + className + "]",
               Array.prototype.toString.call(obj));

  // Custom, non-builtin toStringTags
  obj = {};
  obj[Symbol.toStringTag] = "X" + className;
  assertEquals("[object X" + className + "]",
               Array.prototype.toString.call(obj));

  // With getter
  obj = {};
  Object.defineProperty(obj, Symbol.toStringTag, {
    get: function() { return "X" + className; }
  });
  assertEquals("[object X" + className + "]",
               Array.prototype.toString.call(obj));

  // Undefined toStringTag should return [object className]
  var obj = className === "Arguments" ?
      (function() { return arguments; })() : new global[className];
  obj[Symbol.toStringTag] = undefined;
  assertEquals("[object " + className + "]",
               Array.prototype.toString.call(obj));

  // With getter
  var obj = className === "Arguments" ?
      (function() { return arguments; })() : new global[className];
  Object.defineProperty(obj, Symbol.toStringTag, {
    get: function() { return undefined; }
  });
  assertEquals("[object " + className + "]",
               Array.prototype.toString.call(obj));
}


[
  "Arguments",
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
  assertEquals("[object Object]", Array.prototype.toString.call(obj));

  // With getter
  obj = {};
  Object.defineProperty(obj, Symbol.toStringTag, {
    get: function() { return value; }
  });
  assertEquals("[object Object]", Array.prototype.toString.call(obj));
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


function testArrayToStringPropertyDesc() {
  var desc = Object.getOwnPropertyDescriptor(Object.prototype, "toString");
  assertTrue(desc.writable);
  assertFalse(desc.enumerable);
  assertTrue(desc.configurable);
}
testArrayToStringPropertyDesc();


function testArrayToStringOwnNonStringValue() {
  var obj = Object.defineProperty({}, Symbol.toStringTag, { value: 1 });
  assertEquals("[object Object]", ([]).toString.call(obj));
}
testArrayToStringOwnNonStringValue();


function testArrayToStringBasic() {
  assertEquals("1,2,3", [1,2,3].toString());
  assertEquals(",,3", [,,3].toString());
}
testArrayToStringBasic();


function testArrayToStringObjectWithCallableJoin() {
  var obj = { join: function() { return "CallableJoin"; } };
  assertEquals("CallableJoin", Array.prototype.toString.call(obj));
}
testArrayToStringObjectWithCallableJoin();
