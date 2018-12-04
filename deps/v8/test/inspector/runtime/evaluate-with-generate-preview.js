// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} = InspectorTest.start("Tests that Runtime.evaluate will generate correct previews.");

contextGroup.addScript(
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

var shortTypedArray = new Uint8Array(3);
var longTypedArray = new Uint8Array(500001);
var set = new Set([1, 2, 3]);
var bigSet = new Set();
var mixedSet = new Set();
for (var i = 0; i < 10; i++) {
  bigSet.add(i);
  mixedSet["_prop_" + i] = 1;
  mixedSet.add(i);
}

var deterministicNativeFunction = Math.log;
var parentObj = {};
Object.defineProperty(parentObj, 'propNotNamedProto', {
  get: deterministicNativeFunction,
  set: function() {}
});
var objInheritsGetterProperty = {__proto__: parentObj};
inspector.allowAccessorFormatting(objInheritsGetterProperty);

var arrayWithLongValues = ["a".repeat(101), 2n**401n];
`);

contextGroup.setupInjectedScriptEnvironment();

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
  },

  function testShortTypedArrayPropertiesPreview(next)
  {
    Protocol.Runtime.evaluate({ "expression": "shortTypedArray", "generatePreview": true })
        .then(result => InspectorTest.logMessage(result.result.result.preview))
        .then(next);
  },

  function testLongTypedArrayPropertiesPreview(next)
  {
    Protocol.Runtime.evaluate({ "expression": "longTypedArray", "generatePreview": true })
        .then(result => InspectorTest.logMessage(result.result.result.preview))
        .then(next);
  },

  function testSetPropertiesPreview(next)
  {
    Protocol.Runtime.evaluate({ "expression": "set", "generatePreview": true })
        .then(result => InspectorTest.logMessage(result.result.result.preview))
        .then(next);
  },

  function testBigSetPropertiesPreview(next)
  {
    Protocol.Runtime.evaluate({ "expression": "bigSet", "generatePreview": true })
        .then(result => InspectorTest.logMessage(result.result.result.preview))
        .then(next);
  },

  function testMixedSetPropertiesPreview(next)
  {
    Protocol.Runtime.evaluate({ "expression": "mixedSet", "generatePreview": true })
        .then(result => InspectorTest.logMessage(result.result.result.preview))
        .then(next);
  },

  function testObjInheritsGetterProperty(next)
  {
    Protocol.Runtime.evaluate({ "expression": "objInheritsGetterProperty", "generatePreview": true })
        .then(result => InspectorTest.logMessage(result.result.result.preview))
        .then(next);
  },

  function testObjWithArrayAsProto(next)
  {
    Protocol.Runtime.evaluate({ "expression": "Object.create([1,2])", "generatePreview": true })
        .then(result => InspectorTest.logMessage(result.result.result.preview))
        .then(next);
  },

  function testArrayWithLongValues(next)
  {
    Protocol.Runtime.evaluate({ "expression": "arrayWithLongValues", "generatePreview": true })
        .then(result => InspectorTest.logMessage(result.result.result.preview))
        .then(next);
  }
]);
