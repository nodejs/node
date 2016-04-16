// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --strong-mode --allow-natives-syntax

// TODO(conradw): Track implementation of strong bit for other objects, add
// tests.

function getSloppyObjects() {
  return [(function(){}), ({})];
}

function getStrictObjects() {
  "use strict";
  return [(function(){}), ({})];
}

function getWeakObjects() {
  return getSloppyObjects().concat(getStrictObjects());
}

function getStrongObjects() {
  "use strong";
// Strong functions can't have properties added to them, and will be tested as a
// special case.
  return [({})];
}

function strongFunction() {
  "use strong";
}

function deleteFromObjectSloppy(o) {
  return delete o.foo;
}

function deleteFromObjectKeyedSloppy(o) {
  return delete o["foo"];
}

function deleteFromObjectKeyedVarSloppy(o) {
  var a = "foo";
  return delete o[a];
}

function deleteFromObjectKeyedComputedSloppy(o) {
  var a = "o";
  return delete o["fo" + a];
}

function deleteFromObjectWith(o) {
  with (o) {
    return delete foo;
  }
}

function deleteFromObjectElementSloppy(o) {
  return delete o[0];
}

function deleteFromObjectElementVarSloppy(o) {
  var a = 0;
  return delete o[a];
}

function deleteFromObjectElementSparseSloppy(o) {
  return delete o[100000];
}

function deleteFromObjectElementVarSloppy(o) {
  var a = 0;
  return delete o[a];
}

function deleteFromObjectElementSparseVarSloppy(o) {
  var a = 100000;
  return delete o[a];
}

function deleteFromObjectStrict(o) {
  "use strict";
  return delete o.foo;
}

function deleteFromObjectKeyedStrict(o) {
  "use strict";
  return delete o["foo"];
}

function deleteFromObjectKeyedVarStrict(o) {
  "use strict";
  var a = "foo";
  return delete o[a];
}

function deleteFromObjectKeyedComputedStrict(o) {
  "use strict";
  var a = "o";
  return delete o["fo" + a];
}

function deleteFromObjectElementStrict(o) {
  "use strict";
  return delete o[0];
}

function deleteFromObjectElementSparseStrict(o) {
  "use strict";
  return delete o[100000];
}

function deleteFromObjectElementVarStrict(o) {
  "use strict";
  var a = 0;
  return delete o[a];
}

function deleteFromObjectElementSparseVarStrict(o) {
  "use strict";
  var a = 100000;
  return delete o[a];
}

function testStrongObjectDelete() {
  "use strict";

  let deletePropertyFuncsSloppy = [
    deleteFromObjectSloppy,
    deleteFromObjectKeyedSloppy,
    deleteFromObjectKeyedVarSloppy,
    deleteFromObjectKeyedComputedSloppy,
    deleteFromObjectWith
  ];
  let deletePropertyFuncsStrict = [
    deleteFromObjectStrict,
    deleteFromObjectKeyedStrict,
    deleteFromObjectKeyedVarStrict,
    deleteFromObjectKeyedComputedStrict
  ];
  let deleteElementFuncsSloppy = [
    deleteFromObjectElementSloppy,
    deleteFromObjectElementVarSloppy
  ];
  let deleteElementSparseFuncsSloppy = [
    deleteFromObjectElementSparseSloppy,
    deleteFromObjectElementSparseVarSloppy
  ];
  let deleteElementFuncsStrict = [
    deleteFromObjectElementStrict,
    deleteFromObjectElementVarStrict
  ];
  let deleteElementSparseFuncsStrict = [
    deleteFromObjectElementSparseStrict,
    deleteFromObjectElementSparseVarStrict
  ];
  let deleteFuncs = deletePropertyFuncsSloppy.concat(
    deletePropertyFuncsStrict, deleteElementFuncsSloppy,
    deleteElementSparseFuncsSloppy, deleteElementFuncsStrict,
    deleteElementSparseFuncsStrict);

  for (let deleteFunc of deleteFuncs) {
    assertTrue(deleteFunc(strongFunction));
  }

  let testCasesSloppy = [
    [deletePropertyFuncsSloppy, "foo"],
    [deleteElementFuncsSloppy, "0"],
    [deleteElementSparseFuncsSloppy, "100000"]
  ];

  let testCasesStrict = [
    [deletePropertyFuncsStrict, "foo"],
    [deleteElementFuncsStrict, "0"],
    [deleteElementSparseFuncsStrict, "100000"]
  ];

  let propDescs = [
     {configurable: true, value: "bar"},
     {configurable: true, value: 1},
     {configurable: true, enumerable: true, writable: true, value: "bar"},
     {configurable: true, enumerable: true, writable: true, value: 1},
     {configurable: true, get: (function(){return 0}), set: (function(x){})}
  ];

  for (let propDesc of propDescs) {
    for (let testCase of testCasesSloppy) {
      let name = testCase[1];
      for (let deleteFunc of testCase[0]) {
        for (let o of getWeakObjects()) {
          Object.defineProperty(o, name, propDesc);
          assertTrue(delete o["bar"]);
          assertTrue(delete o[5000]);
          assertTrue(deleteFunc(o));
          assertFalse(o.hasOwnProperty(name));
          %OptimizeFunctionOnNextCall(deleteFunc);
          Object.defineProperty(o, name, propDesc);
          assertTrue(deleteFunc(o));
          assertFalse(o.hasOwnProperty(name));
          %DeoptimizeFunction(deleteFunc);
          Object.defineProperty(o, name, propDesc);
          assertTrue(deleteFunc(o));
          assertFalse(o.hasOwnProperty(name));
        }
        for (let o of getStrongObjects()) {
          Object.defineProperty(o, name, propDesc);
          assertTrue(delete o["bar"]);
          assertTrue(delete o[5000]);
          assertFalse(deleteFunc(o));
          assertTrue(o.hasOwnProperty(name));
          %OptimizeFunctionOnNextCall(deleteFunc);
          assertFalse(deleteFunc(o));
          assertTrue(o.hasOwnProperty(name));
          %DeoptimizeFunction(deleteFunc);
          assertFalse(deleteFunc(o));
          assertTrue(o.hasOwnProperty(name));
        }
      }
    }
    for (let testCase of testCasesStrict) {
      let name = testCase[1];
      for (let deleteFunc of testCase[0]) {
        for (let o of getWeakObjects()) {
          Object.defineProperty(o, name, propDesc);
          assertTrue(delete o["bar"]);
          assertTrue(delete o[5000]);
          assertTrue(deleteFunc(o));
          assertFalse(o.hasOwnProperty(name));
          %OptimizeFunctionOnNextCall(deleteFunc);
          Object.defineProperty(o, name, propDesc);
          assertTrue(deleteFunc(o));
          assertFalse(o.hasOwnProperty(name));
          %DeoptimizeFunction(deleteFunc);
          Object.defineProperty(o, name, propDesc);
          assertTrue(deleteFunc(o));
          assertFalse(o.hasOwnProperty(name));
        }
        for (let o of getStrongObjects()) {
          Object.defineProperty(o, name, propDesc);
          assertTrue(delete o["bar"]);
          assertTrue(delete o[5000]);
          assertThrows(function(){deleteFunc(o)}, TypeError);
          assertTrue(o.hasOwnProperty(name));
          %OptimizeFunctionOnNextCall(deleteFunc);
          assertThrows(function(){deleteFunc(o)}, TypeError);
          assertTrue(o.hasOwnProperty(name));
          %DeoptimizeFunction(deleteFunc);
          assertThrows(function(){deleteFunc(o)}, TypeError);
          assertTrue(o.hasOwnProperty(name));
        }
      }
    }
  }
}
testStrongObjectDelete();
