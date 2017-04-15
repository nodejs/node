// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

print("Tests that Runtime.evaluate will generate correct previews.");

InspectorTest.addScript(
`
var f1 = function(){};

Object.prototype[0] = 'default-first';
var obj = {p1: {a:1}, p2: {b:'foo'}, p3: f1};
Object.defineProperties(obj, {
  p4: {
    get() { return 2 }
  },
  p5: {
    set(x) { return x }
  },
  p6: {
    get() { return 2 },
    set(x) { return x }
  }
});

Array.prototype[0] = 'default-first';
var arr = [,, 1, [2], f1];
Object.defineProperties(arr, {
  5: {
    get() { return 2 }
  },
  6: {
    set(x) { return x }
  },
  7: {
    get() { return 2 },
    set(x) { return x }
  }
});
arr.nonEntryFunction = f1;

var inheritingObj = {};
var inheritingArr = [];
inheritingObj.prototype = obj;
inheritingArr.prototype = arr;
`);

InspectorTest.runTestSuite([
  function testObjectPropertiesPreview(next)
  {
    Protocol.Runtime.evaluate({ "expression": "obj", "generatePreview": true })
        .then(result => InspectorTest.logMessage(result.result.result.preview))
        .then(next);
  },

  function testArrayPropertiesPreview(next)
  {
    Protocol.Runtime.evaluate({ "expression": "arr", "generatePreview": true })
        .then(result => InspectorTest.logMessage(result.result.result.preview))
        .then(next);
  },

  function testInheritingObjectPropertiesPreview(next)
  {
    Protocol.Runtime.evaluate({ "expression": "inheritingObj", "generatePreview": true })
        .then(result => InspectorTest.logMessage(result.result.result.preview))
        .then(next);
  },

  function testInheritingArrayPropertiesPreview(next)
  {
    Protocol.Runtime.evaluate({ "expression": "inheritingArr", "generatePreview": true })
        .then(result => InspectorTest.logMessage(result.result.result.preview))
        .then(next);
  }
]);
