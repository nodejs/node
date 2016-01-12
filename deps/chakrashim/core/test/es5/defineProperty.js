//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

if (typeof (WScript) != "undefined") {
  WScript.LoadScriptFile("..\\UnitTestFramework\\UnitTestFramework.js", "self");
}

var tests = {
  // Note: each test has name (string) and body (function) properties. 
  //       Success is when the body does not throw, failure -- when it throws.

  test01: {
    name: "8.12.9.4.a (variation 1): define generic property, check default attrbitues",
    body: function () {
      var o = helpers.getDummyObject();
      var propertyName = "foo02";
      var pd = {};
      Object.defineProperty(o, propertyName, pd);
      var expected = { writable: false, value: undefined, configurable: false, enumerable: false };
      assert.areEqual(expected, Object.getOwnPropertyDescriptor(o, propertyName), "wrong value of getOwnPropertyDescriptor");
    }
  },

  test02: {
    name: "8.12.9.4.a (variation 2): define data property, check default attrbitues",
    body: function () {
      var o = helpers.getDummyObject();
      var propertyName = "foo03";
      var pd = { value: 0 };
      Object.defineProperty(o, propertyName, pd);
      var expected = { writable: false, value: 0, configurable: false, enumerable: false };
      assert.areEqual(expected, Object.getOwnPropertyDescriptor(o, propertyName), "wrong value of getOwnPropertyDescriptor");
    }
  },

  test03: {
    name: "8.12.9.4.a (variation 3): define generic property by specifying some attributes, check attrbitues",
    body: function () {
      var o = helpers.getDummyObject();
      var propertyName = "foo02_v3";
      var pd = { configurable: true, writable: false };
      Object.defineProperty(o, propertyName, pd);
      var expected = { writable: false, value: undefined, configurable: true, enumerable: false };
      assert.areEqual(expected, Object.getOwnPropertyDescriptor(o, propertyName), "wrong value of getOwnPropertyDescriptor");
    }
  },

  test04: {
    name: "8.12.9.4.b: define accessor property, check default attrbitues",
    body: function () {
      var o = helpers.getDummyObject();
      var propertyName = "foo04";
      var getter = function () { return this.Value };
      var pd = { get: getter };
      Object.defineProperty(o, propertyName, pd);
      var expected = { get: getter, set: undefined, configurable: false, enumerable: false };
      assert.areEqual(expected, Object.getOwnPropertyDescriptor(o, propertyName), "wrong value of getOwnPropertyDescriptor");
    }
  },

  test05: {
    name: "8.12.9.5: re-define property: use descriptor with all fields absent, check that nothing happens to previous descriptor",
    body: function () {
      var propertyName = "foo05";
      var o = { foo05: 1 };
      var pd = {};
      Object.defineProperty(o, propertyName, pd);
      var expected = { writable: true, value: 1, configurable: true, enumerable: true };
      assert.areEqual(expected, Object.getOwnPropertyDescriptor(o, propertyName), "wrong value of getOwnPropertyDescriptor");
    }
  },

  test06: {
    name: "8.12.9.6: re-define property: use equal descriptor with data field, check that nothing happens to previous descriptor",
    body: function () {
      var propertyName = "foo06";
      var o = { foo06: 1 };
      var pd = { value: 1 };
      Object.defineProperty(o, propertyName, pd);
      var expected = { writable: true, value: 1, configurable: true, enumerable: true };
      assert.areEqual(expected, Object.getOwnPropertyDescriptor(o, propertyName), "wrong value of getOwnPropertyDescriptor");
    }
  },

  // where we are:
  // - re-define
  // - desc is not empty
  // - desc and current are not the same

  test07: {
    name: "8.12.9.7.a: re-define property: current descriptor is not configurable and descriptor is configurable, check that it throws TypeError",
    body: function () {
      var o = helpers.getDummyObject();
      var propertyName = "foo07";
      var pd = { value: 0, configurable: false };
      Object.defineProperty(o, propertyName, pd);
      pd = { value: 1, configurable: true };
      assert.throws(function() { Object.defineProperty(o, propertyName, pd); }, TypeError);
    }
  },

  test08: {
    name: "8.12.9.7.b (variation 1): re-define property: current descriptor is not configurable and descriptor enumerable is specified and it's negation of current enumerable, check that it throws TypeError",
    body: function () {
      var o = helpers.getDummyObject();
      var propertyName = "foo08";
      var pd = { value: 0 };
      Object.defineProperty(o, propertyName, pd);
      pd = { value: 1, enumerable: true };
      assert.throws(function() { Object.defineProperty(o, propertyName, pd); }, TypeError);
    }
  },

  test09: {
    name: "8.12.9.7.b (variation 2): re-define property: current descriptor is not configurable and descriptor enumerable is not specified, check that it does not throw",
    body: function () {
      var o = helpers.getDummyObject();
      var propertyName = "foo09";
      var pd = { value: 0, writable: true }; // set writable to true to avoid throw code path.
      Object.defineProperty(o, propertyName, pd);
      pd = { value: 1, writable: false };
      Object.defineProperty(o, propertyName, pd);
      var expected = { writable: false, value: 1, configurable: false, enumerable: false };
      assert.areEqual(expected, Object.getOwnPropertyDescriptor(o, propertyName), "wrong value of getOwnPropertyDescriptor");
    }
  },

  test10: {
    name: "8.12.9.7.b (variation 3): re-define property: current descriptor is not configurable and descriptor enumerable is same as current enumerable, check that it does not throw",
    body: function () {
      var o = helpers.getDummyObject();
      var propertyName = "foo10";
      var pd = { value: 0, writable: true }; // set writable to true to avoid throw code path.
      Object.defineProperty(o, propertyName, pd);
      pd = { value: 1, enumerable: false, writable: false };
      Object.defineProperty(o, propertyName, pd);
      var expected = { writable: false, value: 1, configurable: false, enumerable: false };
      assert.areEqual(expected, Object.getOwnPropertyDescriptor(o, propertyName), "wrong value of getOwnPropertyDescriptor");
    }
  },

  test11: {
    name: "8.12.9.8: re-define property: descriptor is not empty, generic and is different from current",
    body: function () {
      var o = helpers.getDummyObject();
      var propertyName = "foo11";
      var pd = { value: 0, configurable: true };
      Object.defineProperty(o, propertyName, pd);
      pd = { enumerable: true }; // change enumerable to make sure that descriptor is diferent from current.
      Object.defineProperty(o, propertyName, pd);
      var expected = { writable: false, value: 0, configurable: true, enumerable: true };
      assert.areEqual(expected, Object.getOwnPropertyDescriptor(o, propertyName), "wrong value of getOwnPropertyDescriptor");
    }
  },

  // where we are:
  // - re-define
  // - desc is not empty
  // - desc and current are not the same
  // - descriptor.IsData != current.IsData

  test12: {
    name: "8.12.9.9.a: re-define property: descriptor.IsData != current.IsData and current is not configurable, check that it throws TypeError",
    body: function () {
      var o = helpers.getDummyObject();
      var propertyName = "foo12";
      var pd = { value: 0, configurable: false };
      Object.defineProperty(o, propertyName, pd);
      pd = { get: function () { return this.Value; } };
      assert.throws(function() { Object.defineProperty(o, propertyName, pd); }, TypeError);
    }
  },

  test13: {
    name: "8.12.9.9.b (variation 1): re-define property: convert from data to accessor descriptor, check that configurable/enumerable (true) are preserved",
    body: function () {
      var o = helpers.getDummyObject();
      var propertyName = "foo13";
      var pd = { value: 0, configurable: true, enumerable: true };
      Object.defineProperty(o, propertyName, pd);
      var getter = function() { return this.Value; };
      pd = { get: getter };
      Object.defineProperty(o, propertyName, pd);
      var expected = { get: getter, set: undefined, configurable: true, enumerable: true };
      assert.areEqual(expected, Object.getOwnPropertyDescriptor(o, propertyName), "wrong value of getOwnPropertyDescriptor");
    }
  },

  test14: {
    name: "8.12.9.9.b (variation 2): re-define property: convert from data to accessor descriptor, check that enumerable (false) is preserved",
    body: function () {
      var o = helpers.getDummyObject();
      var propertyName = "foo14";
      var pd = { value: 0, configurable: true, enumerable: false };
      Object.defineProperty(o, propertyName, pd);
      var getter = function () { return this.Value; };
      pd = { get: getter };
      Object.defineProperty(o, propertyName, pd);
      var expected = { get: getter, set: undefined, configurable: true, enumerable: false };
      assert.areEqual(expected, Object.getOwnPropertyDescriptor(o, propertyName), "wrong value of getOwnPropertyDescriptor");
    }
  },

  test15: {
    name: "8.12.9.9.b (variation 3): re-define property: convert from data to accessor descriptor, check that configurable/enumerable not preserved when specified by descriptor",
    body: function () {
      var o = helpers.getDummyObject();
      var propertyName = "foo15";
      var pd = { value: 0, configurable: true, enumerable: true };
      Object.defineProperty(o, propertyName, pd);
      var getter = function () { return this.Value; };
      pd = { get: getter, configurable: false };
      Object.defineProperty(o, propertyName, pd);
      var expected = { get: getter, set: undefined, configurable: false, enumerable: true };
      assert.areEqual(expected, Object.getOwnPropertyDescriptor(o, propertyName), "wrong value of getOwnPropertyDescriptor");
    }
  },

  test16: {
    name: "8.12.9.9.c (variation 1): re-define property: convert from accessor to data descriptor, check that configurable/enumerable (true) are preserved",
    body: function () {
      var o = helpers.getDummyObject();
      var propertyName = "foo16";
      var pd = {
        set: function (arg) { helpers.writeln("setter was called"); this.Value = arg; },
        configurable: true,
        enumerable: true
      };
      Object.defineProperty(o, propertyName, pd);
      pd = { value: 1 };
      Object.defineProperty(o, propertyName, pd);
      var expected = { writable: false, value: 1, configurable: true, enumerable: true };
      assert.areEqual(expected, Object.getOwnPropertyDescriptor(o, propertyName), "wrong value of getOwnPropertyDescriptor");
    }
  },

  test17: {
    name: "8.12.9.9.c (variation 2): re-define property: convert from accessor to data descriptor, check that enumerable (false) is preserved",
    body: function () {
      var o = helpers.getDummyObject();
      var propertyName = "foo17";
      var pd = {
        set: function (arg) { helpers.writeln("setter was called"); this.Value = arg; },
        configurable: true,
        enumerable: false
      };
      Object.defineProperty(o, propertyName, pd);
      pd = { value: 1 };
      Object.defineProperty(o, propertyName, pd);
      var expected = { writable: false, value: 1, configurable: true, enumerable: false };
      assert.areEqual(expected, Object.getOwnPropertyDescriptor(o, propertyName), "wrong value of getOwnPropertyDescriptor");
    }
  },

  test18: {
    name: "8.12.9.9.c (variation 3): re-define property: convert from accessor to data descriptor, check that configurable/enumerable (true/false) not preserved when specified by descriptor (false/absent)",
    body: function () {
      var o = helpers.getDummyObject();
      var propertyName = "foo18";
      var pd = {
        set: function (arg) { helpers.writeln("setter was called"); this.Value = arg; },
        configurable: true,
        enumerable: false
      };
      Object.defineProperty(o, propertyName, pd);
      pd = { value: 1, configurable: false };
      Object.defineProperty(o, propertyName, pd);
      // expected: configurable/enumerable = false/false.
      var expected = { writable: false, value: 1, configurable: false, enumerable: false };
      assert.areEqual(expected, Object.getOwnPropertyDescriptor(o, propertyName), "wrong value of getOwnPropertyDescriptor");
    }
  },

  test19: {
    name: "8.12.9.9.c (variation 4): re-define property: convert from accessor to data descriptor, check that configurable/enumerable (true/true) not preserved when specified by descriptor (absent/false)",
    body: function () {
      var o = helpers.getDummyObject();
      var propertyName = "foo19";
      var pd = {
        set: function (arg) { helpers.writeln("setter was called"); this.Value = arg; },
        configurable: true,
        enumerable: true
      };
      Object.defineProperty(o, propertyName, pd);
      pd = { value: 1, enumerable: false };
      Object.defineProperty(o, propertyName, pd);
      // expected: configurable/enumerable = true/false.
      var expected = { writable: false, value: 1, configurable: true, enumerable: false };
      assert.areEqual(expected, Object.getOwnPropertyDescriptor(o, propertyName), "wrong value of getOwnPropertyDescriptor");
    }
  },

  // where we are:
  // - re-define
  // - desc is not empty
  // - desc and current are not the same
  // - descriptor is data, current is data

  test20: {
    name: "8.12.9.10.a (variation 1): re-define data property: current is not configurable/not writable and descriptor writable is absent/value is same",
    body: function () {
      var o = helpers.getDummyObject();
      var propertyName = "foo20";
      var pd = { value: 1 };
      Object.defineProperty(o, propertyName, pd);
      pd = { value: 1 };
      Object.defineProperty(o, propertyName, pd);
      var expected = { writable: false, value: 1, configurable: false, enumerable: false };
      assert.areEqual(expected, Object.getOwnPropertyDescriptor(o, propertyName), "wrong value of getOwnPropertyDescriptor");
    }
  },

  test21: {
    name: "8.12.9.10.a.i: re-define data property: current is not configurable/not writable and descriptor is writable, check that it throws TypeError",
    body: function () {
      var o = helpers.getDummyObject();
      var propertyName = "foo21";
      var pd = { value: 1 };
      Object.defineProperty(o, propertyName, pd);
      pd = { value: 2, writable: true };
      assert.throws(function() { Object.defineProperty(o, propertyName, pd); }, TypeError);
      return true;
    }
  },

  test22: {
    name: "8.12.9.10.a.ii: re-define data property: current is not configurable/not writable and descriptor writable is false and value is different, check that it throws TypeError",
    body: function () {
      var o = helpers.getDummyObject();
      var propertyName = "foo22";
      var pd = { value: 1 };
      Object.defineProperty(o, propertyName, pd);
      pd = { value: 2, writable: false };
      assert.throws(function() { Object.defineProperty(o, propertyName, pd); }, TypeError);
    }
  },

  test23: {
    name: "8.12.9.10.a (variation 2): re-define data property: current is configurable",
    body: function () {
      var propertyName = "foo23";
      var o = { foo23: 1 };
      var pd = { value: 2, writable: false };
      Object.defineProperty(o, propertyName, pd);
      var expected = { writable: false, value: 2, configurable: true, enumerable: true };
      assert.areEqual(expected, Object.getOwnPropertyDescriptor(o, propertyName), "wrong value of getOwnPropertyDescriptor");
    }
  },

// where we are:
// - re-define
// - desc is not empty
// - desc and current are not the same
// - descriptor is accessor, current is accessor

  test24: {
    name: "Test: 8.12.9.11 (variation 1): re-define accessor property: curent configurable is true: valid case",
    body: function () {
      var propertyName = "foo24";
      var o = { 
        get foo24() { return this.Value; }, 
        set foo24(arg) { helpers.writeln("old setter"); this.Value = arg; } 
      };
      var newGetter = function() { return 2; };
      var newSetter = function(arg) { helpers.writeln("new setter"); }
      var pd = { get: newGetter, set: newSetter };
      Object.defineProperty(o, propertyName, pd);
      var expected = { get: newGetter,  set: newSetter, configurable: true, enumerable: true };
      assert.areEqual(expected, Object.getOwnPropertyDescriptor(o, propertyName), "wrong value of getOwnPropertyDescriptor");
    }
  },

  test25: {
    name: "8.12.9.11.a.i: re-define accessor property: curent configurable is false, descriptor specifies setter as different, expect TypeError",
    body: function () {
      var propertyName = "foo25";
      var o = helpers.getDummyObject();
      var pd = { set: function(arg) { helpers.writeln("old setter"); this.Value = arg; } };
      Object.defineProperty(o, propertyName, pd);

      pd = { set: function(arg) { helpers.writeln("new setter"); } };
      assert.throws(function() { Object.defineProperty(o, propertyName, pd); }, TypeError);
    }
  },

  test26: {
    name: "8.12.9.11.a.ii: re-define accessor property: curent configurable is false, descriptor specifies getter as different, expect TypeError",
    body: function () {
      var propertyName = "foo26";
      var o = helpers.getDummyObject();
      var pd = { get: function() { return this.Value; }, };
      Object.defineProperty(o, propertyName, pd);

      pd = { get: function() { helpers.writeln("new getter"); return 2; } };
      assert.throws(function() { Object.defineProperty(o, propertyName, pd); }, TypeError);
    }
  },

  test27: {
    name: "8.12.9.11 (variation 2): re-define accessor property: curent configurable is true and no getter, descriptor specifies getter as undefined, setter as same",
    body: function () {
      var propertyName = "foo27";
      var o = helpers.getDummyObject();
      var setter = function(arg) { helpers.writeln("setter") }; 
      var pd = { set: setter };
      Object.defineProperty(o, propertyName, pd);

      pd = { get: undefined, set: setter };
      Object.defineProperty(o, propertyName, pd);
      var expected = { get: undefined, set: setter, configurable: false, enumerable: false };
      assert.areEqual(expected, Object.getOwnPropertyDescriptor(o, propertyName), "wrong value of getOwnPropertyDescriptor");
    }
  },

  test28: {
    name: "Re-define property from data to accessor property. Make sure that setter is called when setting the value.",
    body: function () {
      // define a data property.
      var propertyName = "foo28";
      var o = helpers.getDummyObject();
      var pd = { value: 1, configurable: true };
      Object.defineProperty(o, propertyName, pd);

      // re-define the property to be accessor property.
      var log = "";
      var getter = function() { log += "getter was called."; return this.Value; }
      var setter = function(arg) { log += "setter was called."; this.Value = arg; }; 
      pd = { get: getter, set: setter };
      Object.defineProperty(o, propertyName, pd);

      // set the value and get it.
      var newValue = 2;
      o[propertyName] = newValue;
      var actualValue = o[propertyName];

      // validate.
      var expected = { get: getter, set: setter, configurable: true, enumerable: false };
      assert.areEqual(expected, Object.getOwnPropertyDescriptor(o, propertyName), "wrong value of getOwnPropertyDescriptor");
      assert.areEqual("setter was called.getter was called.", log, "wrong log");
      assert.areEqual(newValue, actualValue, "wrong value");
    }
  },

  test29: {
    name: "Define property 'length' as accessor property on array: check that it throws TypeError.",
    body: function () {
      assert.throws(
        function() { Object.defineProperty([], "length", {configurable: false, get: function() {return 2;}}); },
        TypeError);
      assert.throws(
        function() { Object.defineProperty(Array.prototype, "length", {configurable: false, get: function() {return 2;}}); },
        TypeError);
    }
  },

  // Where we are: some tests for specific issues.
  test30: {
    name: "Define property with getter specified as undefined, then access the property (WOOB bug 1123281)",
    body: function () {
      var o = helpers.getDummyObject();
      var propertyName = "foo30";
      var pd = { get: undefined };
      Object.defineProperty(o, propertyName, pd);
      assert.areEqual(undefined, o[propertyName]);
    }
  },
  
  test31: {
    name: "Define property with setter specified as undefined, then set the property (WOOB bug 1123281)",
    body: function () {
      var o = helpers.getDummyObject();
      var propertyName = "foo31";
      var pd = { set: undefined };
      Object.defineProperty(o, propertyName, pd);
      o[propertyName] = 1; // Make sure this does not throw.
      assert.areEqual(undefined, o[propertyName]); // Just in case try to access the property.
    }
  },

  test32: {
    name: "Convert data to accessor property with getter specified as undefined, then access the property (WOOB bug 1123281)",
    body: function () {
      var o = helpers.getDummyObject();
      var propertyName = "foo32";
      var pd = { configurable: true, value: 0 };
      Object.defineProperty(o, propertyName, pd);

      pd = { get: undefined };
      Object.defineProperty(o, propertyName, pd);
      assert.areEqual(undefined, o[propertyName]);
    }
  },

  test33: {
    name: "Convert data to accessor property with setter specified as undefined, then set the property (WOOB bug 1123281)",
    body: function () {
      var o = helpers.getDummyObject();
      var propertyName = "foo33";
      var pd = { configurable: true, value: 0 };
      Object.defineProperty(o, propertyName, pd);

      pd = { set: undefined };
      Object.defineProperty(o, propertyName, pd);
      o[propertyName] = 1; // Make sure this does not throw.
      assert.areEqual(undefined, o[propertyName]); // Just in case try to access the property.
    }
  },

  test34: {
    name: "Convert accessor to a data property for non-extensible object (WIN8 bug 463559)",
    body: function () {
      var o = helpers.getDummyObject();
      var propertyName = "x";

      Object.defineProperty(o, propertyName, { 
        get: function() { return 0; },
        set: function(val) { helpers.writeln("setter was called although it shouldn't"); },
        configurable: true
      });
      Object.preventExtensions(o);
      var val = 1;
      Object.defineProperty(o, propertyName, { value: val, });

      var expected = { value: val, configurable: true, enumerable: false, writable: false };
      assert.areEqual(expected, Object.getOwnPropertyDescriptor(o, propertyName), "wrong value of getOwnPropertyDescriptor");
      assert.areEqual(val, o[propertyName], "the property value is wrong");
      assert.areEqual(false, Object.isExtensible(o), "isExtensible() changed");
    }
  },

  // Note: this test irreversibly changes the dummy object (that's important when dummy object is document/window), 
  //       it should in the very end.
  test_last_01: {
    name: "8.12.9.3: define property for non-extensible object, check that it throws TypeError",
    body: function () {
      var o = helpers.getDummyObject();
      Object.preventExtensions(o);
      var propertyName = "foo01";
      var pd = {};
      assert.throws(function() { Object.defineProperty(o, propertyName, pd); }, TypeError);
    }
  }

}; // tests.

testRunner.runTests(tests);
