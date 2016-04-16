// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --strong-mode --allow-natives-syntax

function getSloppyArguments() {
  return arguments;
}

function getObjects() {
  "use strict";
  return [
    {},
    Object(""),
    [],
    (function(){}),
    (class Foo {}),
    getSloppyArguments(),
    arguments,
    new Date(),
    new Uint32Array(0)
  ];
}

function readFromObjectSloppy(o) {
    return o.foo;
}

function readFromObjectKeyedSloppy(o) {
    return o["foo"];
}

function readFromObjectKeyedVarSloppy(o) {
  var a = "foo";
  return o[a];
}

function readFromObjectKeyedComputedSloppy(o) {
  var a = "o";
  return o["fo" + a];
}

function readFromObjectStrong(o) {
  "use strong";
  return o.foo;
}

function readFromObjectKeyedStrong(o) {
  "use strong";
  return o["foo"];
}

function readFromObjectKeyedLetStrong(o) {
  "use strong";
  let a = "foo";
  return o[a];
}

function readFromObjectKeyedComputedStrong(o) {
  "use strong";
  let a = "o";
  return o["fo" + a];
}

function getDescs(x) {
  return [
    {value: x},
    {configurable: true, enumerable: true, writable: true, value: x},
    {configurable: true, enumerable: true, get: (function() {return x}) },
  ];
}

function assertStrongSemantics(func, object) {
  %DeoptimizeFunction(func);
  %ClearFunctionTypeFeedback(func);
  assertThrows(function(){func(object)}, TypeError);
  assertThrows(function(){func(object)}, TypeError);
  assertThrows(function(){func(object)}, TypeError);
  %OptimizeFunctionOnNextCall(func);
  assertThrows(function(){func(object)}, TypeError);
  %DeoptimizeFunction(func);
  assertThrows(function(){func(object)}, TypeError);
}

function assertSloppySemantics(func, object) {
  %DeoptimizeFunction(func);
  %ClearFunctionTypeFeedback(func);
  assertDoesNotThrow(function(){func(object)});
  assertDoesNotThrow(function(){func(object)});
  assertDoesNotThrow(function(){func(object)});
  %OptimizeFunctionOnNextCall(func);
  assertDoesNotThrow(function(){func(object)});
  %DeoptimizeFunction(func);
  assertDoesNotThrow(function(){func(object)});
}

(function () {
  "use strict";

  let goodKeys = [
    "foo"
  ]

  let badKeys = [
    "bar",
    "1",
    "100001",
    "3000000001",
    "5000000001"
  ];

  let values = [
    "string",
    1,
    100001,
    30000000001,
    50000000001,
    NaN,
    {},
    undefined
  ];

  let literals = [0, NaN, true, "string"];

  let badAccessorDescs = [
    { set: (function(){}) },
    { configurable: true, enumerable: true, set: (function(){}) }
  ];

  let readSloppy = [
    readFromObjectSloppy,
    readFromObjectKeyedSloppy,
    readFromObjectKeyedVarSloppy,
    readFromObjectKeyedComputedSloppy
  ];

  let readStrong = [
    readFromObjectStrong,
    readFromObjectKeyedStrong,
    readFromObjectKeyedLetStrong,
    readFromObjectKeyedComputedStrong
  ];

  let dummyProto = {};
  for (let key of goodKeys) {
    Object.defineProperty(dummyProto, key, { value: undefined });
  }

  let dummyAccessorProto = {};
  for (let key of goodKeys) {
    Object.defineProperty(dummyAccessorProto, key, { set: (function(){}) })
  }

  // Attempting to access a property on an object with no defined properties
  // should throw.
  for (let object of getObjects().concat(literals)) {
    for (let func of readStrong) {
      assertStrongSemantics(func, object);
    }
    for (let func of readSloppy) {
      assertSloppySemantics(func, object);
    }
  }
  for (let object of getObjects()) {
    // Accessing a property which is on the prototype chain of the object should
    // not throw.
    object.__proto__ = dummyProto;
    for (let key of goodKeys) {
      for (let func of readStrong.concat(readSloppy)) {
        assertSloppySemantics(func, object);
      }
    }
  }
  // Properties with accessor descriptors missing 'get' should throw on access.
  for (let desc of badAccessorDescs) {
    for (let key of goodKeys) {
      for (let object of getObjects()) {
        Object.defineProperty(object, key, desc);
        for (let func of readStrong) {
          assertStrongSemantics(func, object);
        }
        for (let func of readSloppy) {
          assertSloppySemantics(func, object);
        }
      }
    }
  }
  // The same behaviour should be expected for bad accessor properties on the
  // prototype chain.
  for (let object of getObjects()) {
    object.__proto__ = dummyAccessorProto;
    for (let func of readStrong) {
      assertStrongSemantics(func, object);
    }
    for (let func of readSloppy) {
      assertSloppySemantics(func, object);
    }
  }
  assertThrows(function(){"use strong"; typeof ({}).foo;}, TypeError);
  assertThrows(
    function(){"use strong"; typeof ({}).foo === "undefined"}, TypeError);
})();
