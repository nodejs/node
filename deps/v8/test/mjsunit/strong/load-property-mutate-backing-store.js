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
    // TODO(conradw): uncomment once Object.defineProperty is fixed.
    // new Uint32Array(0)
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

  // After altering the backing store, accessing a missing property should still
  // throw.
  for (let key of badKeys) {
    for (let value of values) {
      for (let desc of getDescs(value)) {
        for (let object of getObjects()) {
          Object.defineProperty(object, key, desc);
          for (let func of readStrong) {
            assertStrongSemantics(func, object);
          }
          for (let func of readSloppy) {
            assertSloppySemantics(func, object);
          }
          // Accessing a property which is on the prototype chain of the object
          // should not throw.
          object.__proto__ = dummyProto;
          for (let key of goodKeys) {
            for (let func of readStrong.concat(readSloppy)) {
              assertSloppySemantics(func, object);
            }
          }
        }
      }
    }
  }
})();
