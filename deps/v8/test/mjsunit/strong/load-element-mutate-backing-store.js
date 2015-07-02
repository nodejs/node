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
  ];
}

// TODO(conradw): add tests for non-inheritance once semantics are implemented.
function getNonInheritingObjects() {
  "use strong";
  return [
    Object(""),
    [],
    // TODO(conradw): uncomment and correct test once Object.defineProperty is
    // fixed.
    // new Uint32Array(0)
  ];
}

function readFromObjectElementSloppy(o) {
  return o[0];
}

function readFromObjectElementSparseSloppy(o) {
  return o[100000];
}

function readFromObjectElementNonSmiSloppy(o) {
  return o[3000000000];
}

function readFromObjectNonIndexSloppy(o) {
  return o[5000000000];
}

function readFromObjectElementVarSloppy(o) {
  var a = 0;
  return o[a];
}

function readFromObjectElementSparseVarSloppy(o) {
  var a = 100000;
  return o[a];
}

function readFromObjectElementNonSmiVarSloppy(o) {
  var a = 3000000000;
  return o[a];
}

function readFromObjectNonIndexVarSloppy(o) {
  var a = 5000000000;
  return o[a];
}

function readFromObjectElementStrong(o) {
  "use strong";
  return o[0];
}

function readFromObjectElementSparseStrong(o) {
  "use strong";
  return o[100000];
}

function readFromObjectElementNonSmiStrong(o) {
  "use strong";
  return o[3000000000];
}

function readFromObjectNonIndexStrong(o) {
  "use strong";
  return o[5000000000];
}

function readFromObjectElementLetStrong(o) {
  "use strong";
  let a = 0;
  return o[a];
}

function readFromObjectElementSparseLetStrong(o) {
  "use strong";
  let a = 100000;
  return o[a];
}

function readFromObjectElementNonSmiLetStrong(o) {
  "use strong";
  let a = 3000000000;
  return o[a];
}

function readFromObjectNonIndexLetStrong(o) {
  "use strong";
  let a = 5000000000;
  return o[a];
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
    "0",
    "100000",
    "3000000000",
    "5000000000"
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
    readFromObjectElementSloppy,
    readFromObjectElementSparseSloppy,
    readFromObjectElementNonSmiSloppy,
    readFromObjectNonIndexSloppy,
    readFromObjectElementVarSloppy,
    readFromObjectElementSparseVarSloppy,
    readFromObjectElementNonSmiVarSloppy,
    readFromObjectNonIndexVarSloppy
  ];

  let readStrong = [
    readFromObjectElementStrong,
    readFromObjectElementSparseStrong,
    readFromObjectElementNonSmiStrong,
    readFromObjectNonIndexStrong,
    readFromObjectElementLetStrong,
    readFromObjectElementSparseLetStrong,
    readFromObjectElementNonSmiLetStrong,
    readFromObjectNonIndexLetStrong
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
        let objects = getObjects();
        let nonInheritingObjects = getNonInheritingObjects();
        for (let object of objects.concat(nonInheritingObjects)) {
          Object.defineProperty(object, key, desc);
          for (let func of readStrong) {
            assertStrongSemantics(func, object);
          }
          for (let func of readSloppy) {
            assertSloppySemantics(func, object);
          }
        }
        for (let object of objects) {
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
