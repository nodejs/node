//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

if (typeof (WScript) != "undefined") {
  WScript.LoadScriptFile("..\\UnitTestFramework\\UnitTestFramework.js", "self");
}

var tests = {
  test01: {
    name: "Make sure that shared type handler is not shared between evolved instances",
    body: function () {
      var propName = "p01";
      var x = helpers.getDummyObject(propName);
      Object.preventExtensions(x);
      var y = helpers.getDummyObject(propName);
      assert.isFalse(Object.isExtensible(x), "Object.isExtensible(x)");
      assert.isTrue (Object.isExtensible(y), "Object.isExtensible(y)");
    }
  },

  test02: {
    name: "Make sure that checks are done when preventExtensions=true for non-extensible object with same layout",
    body: function () {
      var propName = "p02_1";
      var propName2 = "p02_2";
      var f = function f(o) { var tepm = o[propName2]; o[propName2] = 10; };
      var x = helpers.getDummyObject(propName);
      f(x);
      var y = helpers.getDummyObject(propName);
      Object.preventExtensions(y);
      f(y);
      assert.areEqual(undefined, y[propName2], "y has preventExtensions=true -> can't add new property");
    }
  },

  test03: {
    name: "Make sure that after preventExtensions the type can evolve without corrupting the shared type",
    body: function () {
      var propName = "p03";
      var x = helpers.getDummyObject(propName);
      Object.preventExtensions(x);
      delete x[propName];
      var x = helpers.getDummyObject(propName); // new object with same layout as previous one.
      x[propName] = 2;
      Object.preventExtensions(x); // Earlier bug: this was associated a with evolved type.
      var newValue = 3;
      x[propName] = newValue; // Can modify writable property but not on evolved type.
      assert.isFalse(Object.isExtensible(x));
      assert.areEqual(newValue, x[propName]);
    }
  },

  test04: {
    name: "Make sure that after deleting property and then preventExtensions, inline cache is not used so that we can't add the property back",
    body: function () {
      var propName1 = "p04_1";
      var propName2 = "p04_2";
      var x = helpers.getDummyObject(propName1);
      delete x[propName1];
      x[propName2] = propName2;
      Object.preventExtensions(x);
      assert.throws(function() { Object.defineProperty(x, propName1, {value: 1}) }, TypeError);
    }
  },

  test05: {
    name: "Evolve s.d.t.h by redefining a property and make sure on shared instance there is no change",
    body: function () {
      var propName = "p05";
      var x = helpers.getDummyObject(propName);
      Object.seal(x);
      Object.defineProperty(x, propName, {writable: false, value: 100}); // this should succeed
      var y = helpers.getDummyObject(propName);
      Object.seal(y); // Now y shares same s.d.t.h as x used to before defineProperty on x.
      var newValue = 200;
      Object.defineProperty(y, propName, {writable: false, value: newValue}); // this should succeed again (defineProperty on x should not corrupt shared t.h.)
      assert.areEqual(newValue, y[propName]);
    }
  },

  test06: {
    name: "Make sure that preventExtensions/seal/freeze types are using different shared types",
    body: function () {
      var propName = "p06";
      var values = [0, 1, 2, 3];
      var x = helpers.getDummyObject(propName);
      x[propName] = values[0];
      Object.freeze(x);
      x[propName] = values[1];  // this should not work
      var y = helpers.getDummyObject(propName);
      Object.seal(y);
      y[propName] = values[2];   // this should work
      var z = helpers.getDummyObject(propName);
      Object.preventExtensions(z);
      z[propName] = values[3];   // this should work
      assert.areEqual([0, 2, 3], [x[propName], y[propName], z[propName]]);
    }
  },

  test07: {
    name: "Call preventExtensions, seal, freeze on the same instance",
    body: function () {
      var propName1 = "p07_01";
      var propName2 = "p07_02";
      var origVal = 0;
      var x = helpers.getDummyObject(propName1);
      var y = helpers.getDummyObject(propName1);
      x[propName1] = origVal;
      y[propName1] = origVal;
      Object.preventExtensions(x);
      Object.preventExtensions(y);
      Object.seal(y);
      Object.freeze(y);
      var val = 100;
      x[propName1] = val; // This should succeed
      x[propName2] = val; // This should not succeed: can't add new property to non-extensible
      assert.isFalse(Object.isExtensible(x));
      assert.isFalse(Object.isSealed(x));
      assert.areEqual(val, x[propName1], "x should still have read-write properties");
      assert.areEqual(undefined, x[propName2]);
      assert.isTrue(Object.isFrozen(y), "Object.isFrozen(y)");
      y[propName1] = val;
      y[propName2] = val;
      assert.areEqual(origVal,   y[propName1], "y is frozen");
      assert.areEqual(undefined, y[propName2]);
    }
  },

  test08: {
    name: "Attmept to add a numeric property to non-extensible empty object",
    body: function () {
      var numericPropName = 0;
      var x = helpers.getDummyObject();
      Object.preventExtensions(x);
      x[numericPropName] = 0;
      assert.areEqual(undefined, x[numericPropName]);
    }
  },

  test09: {
    name: "Call preventExtensions on object with numeric item, then attempt to add a new numeric item",
    body: function () {
      var propName = "p09";
      var numericPropName = 0;
      var x = helpers.getDummyObject(propName);
      x[numericPropName] = 0;
      Object.preventExtensions(x);
      x[numericPropName + 1] = 1; // Should fail.
      Object.defineProperty(x, numericPropName, {value: 2}); // Another way to add a property, should fail as well.
      assert.areEqual(undefined, x[numericPropName + 1]);
    }
  },

  test10: {
    name: "Inline cache and freeze: make sure that inline cache is not used for frozen type",
    body: function () {
      var newValue = 100;
      var f = function f(o) {
        var temp = o[propName]; // create inline cache for read.
        o[propName] = newValue; // attempt to use inline cache, in particular, for read-only property.
      }
      var propName = "p10";
      var x = helpers.getDummyObject(propName);
      var y = helpers.getDummyObject(propName);
      var z = helpers.getDummyObject(propName);

      Object.freeze(y);
      Object.freeze(z);
      f(x);
      f(y); f(y);
      f(z);

      assert.areEqual(newValue, x[propName], "x[propName]");
      assert.areEqual(0, y[propName], "y[propName]");
      assert.areEqual(0, z[propName], "z[propName]");
    }
  },

} // tests.

testRunner.run(tests);
