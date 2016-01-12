//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

var helpers = function helpers() {
  //private
  var undefinedAsString = "undefined";

  var isInBrowser = function isInBrowser() {
    return typeof (document) !== undefinedAsString;
  };

  return {
    // public
    getDummyObject: function () {
      //return isInBrowser() ? document : {};
      return {};
    },

    writeln: function writeln() {
      var line = "", i;
      for (i = 0; i < arguments.length; i += 1) {
        line = line.concat(arguments[i])
      }
      if (!isInBrowser()) {
        WScript.Echo(line);
      } else {
        document.writeln(line);
        document.writeln("<br/>");
      }
    },

    printObject: function printObject(o) {
      var name;
      for (name in o) {
        this.writeln(name, o.hasOwnProperty(name) ? "" : " (inherited)", ": ", o[name]);
      }
    }
  }
} (); // helpers module.

var testRunner = function testRunner() {
  var executedTestCount = 0;
  var passedTestCount = 0;
  var passName;

  return {
    // Runs provided tests.
    // passes is a collection of {name, prep}, where prep is a function to prepare for the pass.
    // The 'testsToRun' is an object that has enumerable properties,
    // each property is an object that has 'name' and 'body' properties.
    runTests: function runTests(passes, testsToRun) {
      for (var p in passes) {
        var pass = passes[p];
        passName = pass.name;
        if (pass.prep) {
            pass.prep();
        }
        for (var i in testsToRun) {
            var test = tests[i];

            //
            // * If test.disabled (e.g., temp bug), skip it.
            // * If test.pass specifies a pass name, only run it for that pass.
            // * If test.pass not defined, run it for any non "runonce" pass.
            //
            if (!test.disabled && (test.pass === passName || (!test.pass && !pass.runonce))) {
                this.runTest(i, test.name, test.body);
            }
        }
      }

      helpers.writeln("Summary of tests: total executed: ", executedTestCount,
        "; passed: ", passedTestCount, "; failed: ", executedTestCount - passedTestCount);
    },

    // Runs test body catching all exceptions.
    // Result: prints PASSED/FAILED to the output.
    runTest: function runTest(testIndex, testName, testBody) {
      helpers.writeln("*** ", passName, " (", testIndex, "): ", testName);

      var isSuccess = true;
      try {
        testBody();
      } catch (ex) {
        var message = ex.message !== undefined ? ex.message : ex;
        helpers.writeln("Test threw exception: ", message);
        isSuccess = false;
      }
      if (isSuccess) {
        helpers.writeln("PASSED");
        ++passedTestCount;
      } else {
        helpers.writeln("FAILED");
      }

      ++executedTestCount;
    }
  }
}(); // testRunner.

var assert = function assert() {
  // private
  var isObject = function isObject(x) {
    return x instanceof Object && typeof x !== "function";
  };

  var compare = function compare(expected, actual) {
    if (isObject(expected)) {
      if (!isObject(actual)) return "actual is not an object";
      var expectedFieldCount = 0, actualFieldCount = 0;
      for (var i in expected) {
        var compareResult = compare(expected[i], actual[i]);
        if (compareResult !== true) return compareResult;
        ++expectedFieldCount;
      }
      for (var i in actual) {
        ++actualFieldCount;
      }
      if (expectedFieldCount !== actualFieldCount) {
        return "actual has different number of fields than expected";
      }
      return true;
    } else {
      if (isObject(actual)) return "actual is an object";
      if (expected === actual) return true;
      return "expected: " + expected + " actual: " + actual;
    }
  };

  var addMessage = function addMessage(baseMessage, message) {
    if (message !== undefined) {
      baseMessage += ": " + message;
    }
    return baseMessage;
  }

  return {
    // Performs deep comparison of arguments.
    // This works for objects and simple types.
    // TODO: account for other types?
    // TODO: account for missing vs undefined fields.
    areEqual: function areEqual(expected, actual, message) {
      var compareResult = compare(expected, actual);
      if (compareResult !== true) {
        throw addMessage("assert.areEqual failed: " + compareResult, message);
      }
    },

    areNotEqual: function areNotEqual(expected, actual, message) {
      var compareResult = compare(expected, actual);
      if (compareResult === true) {
        throw addMessage("assert.areNotEqual failed", message);
      }
    },

    // Makes sure that the function specified by the 'testFunction' parameter
    // throws the exception specified by the 'expectedException' parameter.
    // Note: currently we check only for specific exception and not "all exceptions derived from specified".
    // Example:
    // assert.throws(function() { eval("{"); }, SyntaxError, "expected SyntaxError")
    throws: function throws(testFunction, expectedException, message) {
      var exception = null;
      try {
        testFunction();
      } catch (ex) {
        exception = ex;
      }
      if (!(exception instanceof Object && exception.constructor === expectedException)) {
        var expectedString = expectedException.toString().replace(/\n/g, "").replace(/.*function (.*)\(.*/g, "$1");
        throw addMessage("assert.throws failed: expected: " + expectedString + ", actual: " + exception, message);
      }
    },

    // Can be used to fail the test.
    fail: function fail(message) {
      throw message;
    }
  }
}(); // assert.

var tests = {
    // Note: each test has name (string) and body (function) properties.
    //       Success is when the body does not throw, failure -- when it throws.

    //---------------------- normal identifier property names -------------------------------
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
        var propertyName = "foo02";
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

    // Note: this test irreversibly changes the dummy object (that's important when dummy object is document/window),
    //       it should in the very end.
    test34: {
      name: "8.12.9.3: define property for non-extensible object, check that it throws TypeError",
      body: function () {
        var o = helpers.getDummyObject();
        Object.preventExtensions(o);
        var propertyName = "foo01";
        var pd = {};
        assert.throws(function() { Object.defineProperty(o, propertyName, pd); }, TypeError);
      }
    },

    //---------------------- numeric property names -------------------------------
    test_101: {
        name: "8.12.9.4.a (variation 1): define generic property, check default attrbitues",
        body: function () {
            var o = helpers.getDummyObject();
            var propertyName = "2";
            var pd = {};
            Object.defineProperty(o, propertyName, pd);
            var expected = { writable: false, value: undefined, configurable: false, enumerable: false };
            assert.areEqual(expected, Object.getOwnPropertyDescriptor(o, propertyName), "wrong value of getOwnPropertyDescriptor");
        }
    },

    test_102: {
        name: "8.12.9.4.a (variation 2): define data property, check default attrbitues",
        body: function () {
            var o = helpers.getDummyObject();
            var propertyName = "3";
            var pd = { value: 0 };
            Object.defineProperty(o, propertyName, pd);
            var expected = { writable: false, value: 0, configurable: false, enumerable: false };
            assert.areEqual(expected, Object.getOwnPropertyDescriptor(o, propertyName), "wrong value of getOwnPropertyDescriptor");
        }
    },

    test_103: {
        name: "8.12.9.4.a (variation 3): define generic property by specifying some attributes, check attrbitues",
        body: function () {
            var o = helpers.getDummyObject();
            var propertyName = "2";
            var pd = { configurable: true, writable: false };
            Object.defineProperty(o, propertyName, pd);
            var expected = { writable: false, value: undefined, configurable: true, enumerable: false };
            assert.areEqual(expected, Object.getOwnPropertyDescriptor(o, propertyName), "wrong value of getOwnPropertyDescriptor");
        }
    },

    test_104: {
        name: "8.12.9.4.b: define accessor property, check default attrbitues",
        body: function () {
            var o = helpers.getDummyObject();
            var propertyName = "4";
            var getter = function () { return this.Value };
            var pd = { get: getter };
            Object.defineProperty(o, propertyName, pd);
            var expected = { get: getter, set: undefined, configurable: false, enumerable: false };
            assert.areEqual(expected, Object.getOwnPropertyDescriptor(o, propertyName), "wrong value of getOwnPropertyDescriptor");
        }
    },

    test_105: {
        name: "8.12.9.5: re-define property: use descriptor with all fields absent, check that nothing happens to previous descriptor",
        body: function () {
            var propertyName = "5";
            var o = { 5: 1 };
            var pd = {};
            Object.defineProperty(o, propertyName, pd);
            var expected = { writable: true, value: 1, configurable: true, enumerable: true };
            assert.areEqual(expected, Object.getOwnPropertyDescriptor(o, propertyName), "wrong value of getOwnPropertyDescriptor");
        }
    },

    test_106: {
        name: "8.12.9.6: re-define property: use equal descriptor with data field, check that nothing happens to previous descriptor",
        body: function () {
            var propertyName = "6";
            var o = { 6: 1 };
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

    test_107: {
        name: "8.12.9.7.a: re-define property: current descriptor is not configurable and descriptor is configurable, check that it throws TypeError",
        body: function () {
            var o = helpers.getDummyObject();
            var propertyName = "7";
            var pd = { value: 0, configurable: false };
            Object.defineProperty(o, propertyName, pd);
            pd = { value: 1, configurable: true };
            assert.throws(function () { Object.defineProperty(o, propertyName, pd); }, TypeError);
        }
    },

    test_108: {
        name: "8.12.9.7.b (variation 1): re-define property: current descriptor is not configurable and descriptor enumerable is specified and it's negation of current enumerable, check that it throws TypeError",
        body: function () {
            var o = helpers.getDummyObject();
            var propertyName = "8";
            var pd = { value: 0 };
            Object.defineProperty(o, propertyName, pd);
            pd = { value: 1, enumerable: true };
            assert.throws(function () { Object.defineProperty(o, propertyName, pd); }, TypeError);
        }
    },

    test_109: {
        name: "8.12.9.7.b (variation 2): re-define property: current descriptor is not configurable and descriptor enumerable is not specified, check that it does not throw",
        body: function () {
            var o = helpers.getDummyObject();
            var propertyName = "9";
            var pd = { value: 0, writable: true }; // set writable to true to avoid throw code path.
            Object.defineProperty(o, propertyName, pd);
            pd = { value: 1, writable: false };
            Object.defineProperty(o, propertyName, pd);
            var expected = { writable: false, value: 1, configurable: false, enumerable: false };
            assert.areEqual(expected, Object.getOwnPropertyDescriptor(o, propertyName), "wrong value of getOwnPropertyDescriptor");
        }
    },

    test_110: {
        name: "8.12.9.7.b (variation 3): re-define property: current descriptor is not configurable and descriptor enumerable is same as current enumerable, check that it does not throw",
        body: function () {
            var o = helpers.getDummyObject();
            var propertyName = "10";
            var pd = { value: 0, writable: true }; // set writable to true to avoid throw code path.
            Object.defineProperty(o, propertyName, pd);
            pd = { value: 1, enumerable: false, writable: false };
            Object.defineProperty(o, propertyName, pd);
            var expected = { writable: false, value: 1, configurable: false, enumerable: false };
            assert.areEqual(expected, Object.getOwnPropertyDescriptor(o, propertyName), "wrong value of getOwnPropertyDescriptor");
        }
    },

    test_111: {
        name: "8.12.9.8: re-define property: descriptor is not empty, generic and is different from current",
        body: function () {
            var o = helpers.getDummyObject();
            var propertyName = "11";
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

    test_112: {
        name: "8.12.9.9.a: re-define property: descriptor.IsData != current.IsData and current is not configurable, check that it throws TypeError",
        body: function () {
            var o = helpers.getDummyObject();
            var propertyName = "12";
            var pd = { value: 0, configurable: false };
            Object.defineProperty(o, propertyName, pd);
            pd = { get: function () { return this.Value; } };
            assert.throws(function () { Object.defineProperty(o, propertyName, pd); }, TypeError);
        }
    },

    test_113: {
        name: "8.12.9.9.b (variation 1): re-define property: convert from data to accessor descriptor, check that configurable/enumerable (true) are preserved",
        body: function () {
            var o = helpers.getDummyObject();
            var propertyName = "13";
            var pd = { value: 0, configurable: true, enumerable: true };
            Object.defineProperty(o, propertyName, pd);
            var getter = function () { return this.Value; };
            pd = { get: getter };
            Object.defineProperty(o, propertyName, pd);
            var expected = { get: getter, set: undefined, configurable: true, enumerable: true };
            assert.areEqual(expected, Object.getOwnPropertyDescriptor(o, propertyName), "wrong value of getOwnPropertyDescriptor");
        }
    },

    test_114: {
        name: "8.12.9.9.b (variation 2): re-define property: convert from data to accessor descriptor, check that enumerable (false) is preserved",
        body: function () {
            var o = helpers.getDummyObject();
            var propertyName = "14";
            var pd = { value: 0, configurable: true, enumerable: false };
            Object.defineProperty(o, propertyName, pd);
            var getter = function () { return this.Value; };
            pd = { get: getter };
            Object.defineProperty(o, propertyName, pd);
            var expected = { get: getter, set: undefined, configurable: true, enumerable: false };
            assert.areEqual(expected, Object.getOwnPropertyDescriptor(o, propertyName), "wrong value of getOwnPropertyDescriptor");
        }
    },

    test_115: {
        name: "8.12.9.9.b (variation 3): re-define property: convert from data to accessor descriptor, check that configurable/enumerable not preserved when specified by descriptor",
        body: function () {
            var o = helpers.getDummyObject();
            var propertyName = "15";
            var pd = { value: 0, configurable: true, enumerable: true };
            Object.defineProperty(o, propertyName, pd);
            var getter = function () { return this.Value; };
            pd = { get: getter, configurable: false };
            Object.defineProperty(o, propertyName, pd);
            var expected = { get: getter, set: undefined, configurable: false, enumerable: true };
            assert.areEqual(expected, Object.getOwnPropertyDescriptor(o, propertyName), "wrong value of getOwnPropertyDescriptor");
        }
    },

    test_116: {
        name: "8.12.9.9.c (variation 1): re-define property: convert from accessor to data descriptor, check that configurable/enumerable (true) are preserved",
        body: function () {
            var o = helpers.getDummyObject();
            var propertyName = "16";
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

    test_117: {
        name: "8.12.9.9.c (variation 2): re-define property: convert from accessor to data descriptor, check that enumerable (false) is preserved",
        body: function () {
            var o = helpers.getDummyObject();
            var propertyName = "17";
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

    test_118: {
        name: "8.12.9.9.c (variation 3): re-define property: convert from accessor to data descriptor, check that configurable/enumerable (true/false) not preserved when specified by descriptor (false/absent)",
        body: function () {
            var o = helpers.getDummyObject();
            var propertyName = "18";
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

    test_119: {
        name: "8.12.9.9.c (variation 4): re-define property: convert from accessor to data descriptor, check that configurable/enumerable (true/true) not preserved when specified by descriptor (absent/false)",
        body: function () {
            var o = helpers.getDummyObject();
            var propertyName = "19";
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

    test_120: {
        name: "8.12.9.10.a (variation 1): re-define data property: current is not configurable/not writable and descriptor writable is absent/value is same",
        body: function () {
            var o = helpers.getDummyObject();
            var propertyName = "20";
            var pd = { value: 1 };
            Object.defineProperty(o, propertyName, pd);
            pd = { value: 1 };
            Object.defineProperty(o, propertyName, pd);
            var expected = { writable: false, value: 1, configurable: false, enumerable: false };
            assert.areEqual(expected, Object.getOwnPropertyDescriptor(o, propertyName), "wrong value of getOwnPropertyDescriptor");
        }
    },

    test_121: {
        name: "8.12.9.10.a.i: re-define data property: current is not configurable/not writable and descriptor is writable, check that it throws TypeError",
        body: function () {
            var o = helpers.getDummyObject();
            var propertyName = "21";
            var pd = { value: 1 };
            Object.defineProperty(o, propertyName, pd);
            pd = { value: 2, writable: true };
            assert.throws(function () { Object.defineProperty(o, propertyName, pd); }, TypeError);
            return true;
        }
    },

    test_122: {
        name: "8.12.9.10.a.ii: re-define data property: current is not configurable/not writable and descriptor writable is false and value is different, check that it throws TypeError",
        body: function () {
            var o = helpers.getDummyObject();
            var propertyName = "22";
            var pd = { value: 1 };
            Object.defineProperty(o, propertyName, pd);
            pd = { value: 2, writable: false };
            assert.throws(function () { Object.defineProperty(o, propertyName, pd); }, TypeError);
        }
    },

    test_123: {
        name: "8.12.9.10.a (variation 2): re-define data property: current is configurable",
        body: function () {
            var propertyName = "23";
            var o = { 23: 1 };
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

    test_124: {
        name: "Test: 8.12.9.11 (variation 1): re-define accessor property: curent configurable is true: valid case",
        body: function () {
          var propertyName = "24";
          var o = {
            get 24() { return this.Value; },
            set 24(arg) { helpers.writeln("old setter"); this.Value = arg; }
          };
          var newGetter = function() { return 2; };
          var newSetter = function(arg) { helpers.writeln("new setter"); }
          var pd = { get: newGetter, set: newSetter };
          Object.defineProperty(o, propertyName, pd);
          var expected = { get: newGetter,  set: newSetter, configurable: true, enumerable: true };
          assert.areEqual(expected, Object.getOwnPropertyDescriptor(o, propertyName), "wrong value of getOwnPropertyDescriptor");
        }
    },

    test_125: {
        name: "8.12.9.11.a.i: re-define accessor property: curent configurable is false, descriptor specifies setter as different, expect TypeError",
        body: function () {
            var propertyName = "25";
            var o = helpers.getDummyObject();
            var pd = { set: function (arg) { helpers.writeln("old setter"); this.Value = arg; } };
            Object.defineProperty(o, propertyName, pd);

            pd = { set: function (arg) { helpers.writeln("new setter"); } };
            assert.throws(function () { Object.defineProperty(o, propertyName, pd); }, TypeError);
        }
    },

    test_126: {
        name: "8.12.9.11.a.ii: re-define accessor property: curent configurable is false, descriptor specifies getter as different, expect TypeError",
        body: function () {
            var propertyName = "26";
            var o = helpers.getDummyObject();
            var pd = { get: function () { return this.Value; } };
            Object.defineProperty(o, propertyName, pd);

            pd = { get: function () { helpers.writeln("new getter"); return 2; } };
            assert.throws(function () { Object.defineProperty(o, propertyName, pd); }, TypeError);
        }
    },

    test_127: {
        name: "8.12.9.11 (variation 2): re-define accessor property: curent configurable is true and no getter, descriptor specifies getter as undefined, setter as same",
        body: function () {
            var propertyName = "27";
            var o = helpers.getDummyObject();
            var setter = function (arg) { helpers.writeln("setter") };
            var pd = { set: setter };
            Object.defineProperty(o, propertyName, pd);

            pd = { get: undefined, set: setter };
            Object.defineProperty(o, propertyName, pd);
            var expected = { get: undefined, set: setter, configurable: false, enumerable: false };
            assert.areEqual(expected, Object.getOwnPropertyDescriptor(o, propertyName), "wrong value of getOwnPropertyDescriptor");
        }
    },

    test_128: {
        name: "Re-define property from data to accessor property. Make sure that setter is called when setting the value.",
        body: function () {
            // define a data property.
            var propertyName = "28";
            var o = helpers.getDummyObject();
            var pd = { value: 1, configurable: true };
            Object.defineProperty(o, propertyName, pd);

            // re-define the property to be accessor property.
            var log = "";
            var getter = function () { log += "getter was called."; return this.Value; }
            var setter = function (arg) { log += "setter was called."; this.Value = arg; };
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

    test_129: {
        name: "Define property 'length' as accessor property on array: check that it throws TypeError.",
        body: function () {
            assert.throws(
        function () { Object.defineProperty([], "length", { configurable: false, get: function () { return 2; } }); },
        TypeError);
            assert.throws(
        function () { Object.defineProperty(Array.prototype, "length", { configurable: false, get: function () { return 2; } }); },
        TypeError);
        }
    },

    // Where we are: some tests for specific issues.
    test_130: {
        name: "Define property with getter specified as undefined, then access the property (WOOB bug 1123281)",
        body: function () {
            var o = helpers.getDummyObject();
            var propertyName = "30";
            var pd = { get: undefined };
            Object.defineProperty(o, propertyName, pd);
            assert.areEqual(undefined, o[propertyName]);
        }
    },

    test_131: {
        name: "Define property with setter specified as undefined, then set the property (WOOB bug 1123281)",
        body: function () {
            var o = helpers.getDummyObject();
            var propertyName = "31";
            var pd = { set: undefined };
            Object.defineProperty(o, propertyName, pd);
            o[propertyName] = 1; // Make sure this does not throw.
            assert.areEqual(undefined, o[propertyName]); // Just in case try to access the property.
        }
    },

    test_132: {
        name: "Convert data to accessor property with getter specified as undefined, then access the property (WOOB bug 1123281)",
        body: function () {
            var o = helpers.getDummyObject();
            var propertyName = "32";
            var pd = { configurable: true, value: 0 };
            Object.defineProperty(o, propertyName, pd);

            pd = { get: undefined };
            Object.defineProperty(o, propertyName, pd);
            assert.areEqual(undefined, o[propertyName]);
        }
    },

    test_133: {
        name: "Convert data to accessor property with setter specified as undefined, then set the property (WOOB bug 1123281)",
        body: function () {
            var o = helpers.getDummyObject();
            var propertyName = "33";
            var pd = { configurable: true, value: 0 };
            Object.defineProperty(o, propertyName, pd);

            pd = { set: undefined };
            Object.defineProperty(o, propertyName, pd);
            o[propertyName] = 1; // Make sure this does not throw.
            assert.areEqual(undefined, o[propertyName]); // Just in case try to access the property.
        }
    },

    // Note: this test irreversibly changes the dummy object (that's important when dummy object is document/window),
    //       it should in the very end.
    test_134: {
        name: "8.12.9.3: define property for non-extensible object, check that it throws TypeError",
        body: function () {
            var o = helpers.getDummyObject();
            Object.preventExtensions(o);
            var propertyName = "1";
            var pd = {};
            assert.throws(function () { Object.defineProperty(o, propertyName, pd); }, TypeError);
        }
    },

    // --------------------- misc adhoc tests -------------------------------------
    test_301: {
        name: "set property whose writable is false",
        body: function() {
            var o = helpers.getDummyObject();
            var propName = 17;
            o[propName] = 100;
            Object.defineProperty(o, propName, {writable: false});
            o[propName] = 200; // should have no effect
            assert.areEqual(100, o[propName]);
        }
    },

    test_302: {
        name: "delete index property",
        body: function() {
            var o = helpers.getDummyObject();
            var propName = 123;

            assert.areEqual(true, delete o[1], "delete non-exist property should return true");

            o[propName] = 123;
            assert.areEqual(true, delete o[propName], "delete this property should return true");
            assert.areEqual(undefined, o[propName], "deleted property value should become undefined");

            Object.defineProperty(o, propName, {get:function(){return 123;}, configurable: true});
            assert.areEqual(123, o[propName], "Property value should be from getter");
            assert.areEqual(true, delete o[propName], "delete this property should return true");
            assert.areEqual(undefined, o[propName], "deleted property value should become undefined");

            Object.defineProperty(o, propName, {value: 123, configurable: false});
            assert.areEqual(123, o[propName], "Property value should be the value");
            assert.areEqual(false, delete o[propName], "delete this property should return false, not configurable");
            assert.areEqual(123, o[propName], "Property value should not be changed");
        }
    },

    test_303: {
        name: "delete a data property then set",
        body: function() {
            var o = helpers.getDummyObject();
            var propName = 303;
            Object.defineProperty(o, propName, {
                value: 100,
                configurable: true
            });
            assert.areEqual(delete o[propName], true, "delete should succeed on configurable data property");
            o[propName] = 200;
            assert.areEqual(200, o[propName]);
        }
    },
    test_304: {
        name: "delete a getter property then set",
        body: function() {
            var o = helpers.getDummyObject();
            var propName = 303;
            Object.defineProperty(o, propName, {
                get: function() { return 100; },
                configurable: true
            });
            assert.areEqual(true, delete o[propName], "delete should succeed on configurable accessor property");
            o[propName] = 200;
            assert.areEqual(200, o[propName]);
        }
    },
    test_305: {
        name: "delete a setter property then set",
        body: function() {
            var o = helpers.getDummyObject();
            var propName = 303;
            Object.defineProperty(o, propName, {
                set: function(arg) { return 100; },
                configurable: true
            });
            assert.areEqual(true, delete o[propName], "delete should succeed on configurable accessor property");
            o[propName] = 200;
            assert.areEqual(200, o[propName]);
        }
    },

    test_306: {
        name: "Set a property while prototype has a getter",
        body: function() {
            var propName = "abc";
            try {
                Object.defineProperty(Object.prototype, propName, {
                    get: function () { return 100; },
                    configurable: true
                });

                var o = helpers.getDummyObject();
                o[propName] = 200; // should have no effect since proto only has a getter
                assert.areEqual(100, o[propName]);
            } finally {
                delete Object.prototype[propName];
            }
        }
    },
    test_306_i: {
        name: "Set a property while prototype has a getter",
        body: function() {
            var propName = "306"; // Without quote it fails on array. Coverred by 310_i
            try {
                Object.defineProperty(Object.prototype, propName, {
                    get: function () { return 100; },
                    configurable: true
                });

                var o = helpers.getDummyObject();
                o[propName] = 200; // should have no effect since proto only has a getter
                assert.areEqual(100, o[propName]);
            } finally {
                delete Object.prototype[propName];
            }
        }
    },

    test_307: {
        name: "Define a property while prototype has a getter",
        body: function() {
            var propName = "abc";
            try {
                Object.defineProperty(Object.prototype, propName, {
                    get: function () { return 100; },
                    configurable: true
                });

                var o = helpers.getDummyObject();
                Object.defineProperty(o, propName, { value: 200 });
                assert.areEqual(200, o[propName]); //DefineOwnProperty succeeds
            } finally {
                delete Object.prototype[propName];
            }
        }
    },
    test_307_i: {
        name: "Define a property while prototype has a getter",
        body: function() {
            var propName = 307;
            try {
                Object.defineProperty(Object.prototype, propName, {
                    get: function () { return 100; },
                    configurable: true
                });

                var o = helpers.getDummyObject();
                Object.defineProperty(o, propName, { value: 200 });
                assert.areEqual(200, o[propName]); //DefineOwnProperty succeeds
            } finally {
                delete Object.prototype[propName];
            }
        }
    },

    test_308: {
        disabled: true, // !!! Disable due to bug (to be opened) causing assertion !!!
        pass: "misc",
        name: "Set a property via object literal while prototype has a getter",
        body: function() {
            var propName = "abc";
            try {
                Object.defineProperty(Object.prototype, propName, {
                    get: function () { return 100; },
                    configurable: true
                });

                var o = {abc: 200};
                assert.areEqual(100, o[propName]);
            } finally {
                delete Object.prototype[propName];
            }
        }
    },
    test_308_i: {
        pass: "misc",
        name: "Set a property via object literal while prototype has a getter",
        body: function() {
            var propName = 308;
            try {
                Object.defineProperty(Object.prototype, propName, {
                    get: function () { return 100; },
                    configurable: true
                });

                var o = {308: 200}; // succeeds since object literals do not check prototypes
                assert.areEqual(200, o[propName]);
            } finally {
                delete Object.prototype[propName];
            }
        }
    },

    test_309_i: {
        disabled: true, // !!! Disabled, Array doesn't honor prototype element attribute/getter/setter !!!
        pass: "misc",
        name: "Set a property while prototype property is not writable",
        body: function() {
            try {
                Object.defineProperty(Object.prototype, 1, {
                    value: 100,
                    writable: false,
                    configurable: true
                });

                var o = [];
                assert.areEqual(100, o[1]);
                o[1] = 200; // should have no effect since proto[1] is not writable
                assert.areEqual(100, o[1]);
            } finally {
                delete Object.prototype[1];
            }
        }
    },

    test_310_i: {
        disabled: true, // !!! Disabled, Array doesn't honor prototype element attribute/getter/setter !!!
        pass: "misc",
        name: "Set a property while prototype property is a getter",
        body: function() {
            try {
                Object.defineProperty(Object.prototype, 1, {
                    get: function () { return 100; },
                    configurable: true
                });

                var o = [];
                assert.areEqual(100, o[1]);
                o[1] = 200; // should have no effect since proto[1] has only getter
                assert.areEqual(100, o[1]);
            } finally {
                delete Object.prototype[1];
            }
        }
    },

    test_311_i: {
        disabled: true, // !!! Disabled, Array doesn't honor prototype element attribute/getter/setter !!!
        pass: "misc",
        name: "Set a property while prototype property is getter/setter",
        body: function() {
            try {
                var tmp = 100;
                Object.defineProperty(Object.prototype, 1, {
                    get: function () { return tmp; },
                    set: function(arg) { tmp = arg + 300; },
                    configurable: true
                });

                var o = [];
                assert.areEqual(100, o[1]);
                o[1] = 200; // should call setter
                assert.areEqual(500, o[1]);
            } finally {
                delete Object.prototype[1];
            }
        }
    },

    test_312_i: {
        name: "Test getter/setter on prototype receives the right this arg",
        body: function() {
            try {
                var propName = "1"; //avoid array fast path for now
                Object.prototype.tmp = 123;
                Object.defineProperty(Object.prototype, propName, {
                    get: function () { return this.tmp; },
                    set: function (arg) { this.tmp = arg + 300; },
                    configurable: true
                });

                var o = helpers.getDummyObject();
                assert.areEqual(123, o[propName], "Should read data on prototype");
                o[propName] = 200; // should call proto setter on o
                assert.areEqual(500, o.tmp, "setter should set data on o");
                assert.areEqual(500, o[propName], "Should read data on o");
                assert.areEqual(123, Object.prototype.tmp, "proto data unchanged");
            } finally {
                delete Object.prototype[propName];
                delete Object.prototype.tmp;
            }
        }
    },
    test_312a_i: {
        name: "Test getter on prototype receives the right this arg",
        body: function() {
            try {
                var propName = "1"; //avoid array fast path for now
                Object.prototype.tmp = 123;
                Object.defineProperty(Object.prototype, propName, {
                    get: function () { return this.tmp; },
                    configurable: true
                });

                var o = helpers.getDummyObject();
                o.length = 10; // Makes propName in length range, also prepare for indexOf
                assert.areEqual(123, o[propName], "Should read data on prototype");
                o.tmp = 456;
                assert.areEqual(456, o[propName], "Should read data on o");
                var i = Array.prototype.indexOf.apply(o, [456]);
                assert.areEqual(propName, i.toString(), "getter should find data on o, not on prototype!");
            } finally {
                delete Object.prototype[propName];
                delete Object.prototype.tmp;
            }
        }
    },

    test_313_i: {
        name: "preventExtensions with index property",
        body: function() {
            var o = helpers.getDummyObject();
            o[1] = 1;
            assert.areEqual(1, o[1]);
            assert.areEqual(true, Object.isExtensible(o), "default is extensible");
            assert.areEqual(false, Object.isSealed(o), "default not sealed");
            assert.areEqual(false, Object.isFrozen(o), "default not frozen");

            Object.preventExtensions(o);
            assert.areEqual(false, Object.isExtensible(o), "now NOT extensible");
            assert.areEqual(false, Object.isSealed(o), "still not sealed, o[1] configurable");
            assert.areEqual(false, Object.isFrozen(o), "still not frozen, o[1] configurable");

            o[1] = 11; // should succeed
            assert.areEqual(11, o[1], "write should succeed");
            o[2] = 2; // should fail
            assert.areEqual(undefined, o[2], "extend should fail");

            // verify unchanged
            assert.areEqual(false, Object.isExtensible(o), "extensible not changed");
            assert.areEqual(false, Object.isSealed(o), "sealed not changed");
            assert.areEqual(false, Object.isFrozen(o), "frozen not changed");
        }
    },
    test_314_i: {
        name: "seal with index property",
        body: function() {
            var o = helpers.getDummyObject();
            o[1] = 1;

            Object.seal(o);
            assert.areEqual(false, Object.isExtensible(o), "now NOT extensible");
            assert.areEqual(true, Object.isSealed(o), "now IS sealed");
            assert.areEqual(false, Object.isFrozen(o), "still not frozen, o[1] writable");

            o[1] = 11; // should succeed
            assert.areEqual(11, o[1], "write should succeed");
            assert.areEqual(false, delete o[1], "delete should fail, object sealed");
            assert.areEqual(11, o[1], "delete should fail");
            o[2] = 2; // should fail
            assert.areEqual(undefined, o[2], "extend should fail");

            // verify unchanged
            assert.areEqual(false, Object.isExtensible(o), "extensible not changed");
            assert.areEqual(true, Object.isSealed(o), "sealed not changed");
            assert.areEqual(false, Object.isFrozen(o), "frozen not changed");
        }
    },
    test_315_i: {
        name: "freeze with index property",
        body: function() {
            var o = helpers.getDummyObject();
            o[1] = 1;

            Object.freeze(o);
            assert.areEqual(false, Object.isExtensible(o), "now NOT extensible");
            assert.areEqual(true, Object.isSealed(o), "now IS sealed");
            assert.areEqual(true, Object.isFrozen(o), "now IS frozen");

            o[1] = 11; // should fail
            assert.areEqual(1, o[1], "write should fail");
            assert.areEqual(false, delete o[1], "delete should fail, object sealed");
            assert.areEqual(1, o[1], "delete should fail");
            o[2] = 2; // should fail
            assert.areEqual(undefined, o[2], "extend should fail");

            // verify unchanged
            assert.areEqual(false, Object.isExtensible(o), "extensible not changed");
            assert.areEqual(true, Object.isSealed(o), "sealed not changed");
            assert.areEqual(true, Object.isFrozen(o), "frozen not changed");
        }
    },

    test_316_i: {
        name: "preventExtensions on empty object -> isSealed and isFrozen",
        body: function() {
            var o = helpers.getDummyObject();
            Object.preventExtensions(o); // Haven't set any item yet, objectArray is null
            assert.areEqual(false, Object.isExtensible(o), "NOT extensible");
            assert.areEqual(true, Object.isSealed(o), "IS sealed");
            assert.areEqual(true, Object.isFrozen(o) || Array.isArray(o), "IS frozen, unless isArray (length writable)");

            o[1] = 11;
            assert.areEqual(undefined, o[1], "Write failed, not extensible");
        }
    },
    test_317_i: {
        name: "preventExtensions on object with an accessor -> isSealed and isFrozen",
        body: function() {
            var o = helpers.getDummyObject();
            var propName = 123;
            Object.defineProperty(o, propName, {get: function(){ return "123"; }, configurable: false});
            Object.preventExtensions(o);
            assert.areEqual(false, Object.isExtensible(o), "NOT extensible");
            assert.areEqual(true, Object.isSealed(o), "IS sealed");
            assert.areEqual(true, Object.isFrozen(o) || Array.isArray(o), "IS frozen, unless isArray (length writable)");

            o[1] = 11;
            assert.areEqual(undefined, o[1], "Write failed, not extensible");
            assert.areEqual(false, delete o[propName], "delete should fail, not configurable");
            assert.areEqual("123", o[propName], "delete failed, not configurable");
        }
    },
    test_318_i: {
        name: "preventExtensions on object with data -> isSealed and isFrozen",
        body: function() {
            var o = helpers.getDummyObject();
            var propName = 123;
            Object.defineProperty(o, propName, {value: 456, configurable: false, writable: true});
            Object.preventExtensions(o);
            assert.areEqual(false, Object.isExtensible(o), "NOT extensible");
            assert.areEqual(true, Object.isSealed(o), "IS sealed");
            assert.areEqual(false, Object.isFrozen(o), "NOT frozen, data writable");
        }
    },
    test_319_i: {
        name: "preventExtensions on object with data -> isSealed and isFrozen",
        body: function() {
            var o = helpers.getDummyObject();
            var propName = 123;
            Object.defineProperty(o, propName, {value: 456, configurable: false, writable: false});
            Object.preventExtensions(o);
            assert.areEqual(false, Object.isExtensible(o), "NOT extensible");
            assert.areEqual(true, Object.isSealed(o), "IS sealed");
            assert.areEqual(true, Object.isFrozen(o) || Array.isArray(o), "IS frozen, unless isArray (length writable)");
        }
    },
    test_320_i: {
        name: "preventExtensions on object with data -> isSealed and isFrozen",
        body: function() {
            var o = helpers.getDummyObject();
            var propName = 123;
            Object.defineProperty(o, propName, {value: 456, configurable: false, writable: false});
            o[234] = 345;
            Object.preventExtensions(o);
            assert.areEqual(false, Object.isExtensible(o), "NOT extensible");
            assert.areEqual(false, Object.isSealed(o), "NOT sealed, 234 configurable");
            assert.areEqual(false, Object.isFrozen(o), "NOT frozen, 234 configurable/writable");
        }
    },

    test_321_i: {
        name: "Test prototype value is used in sort",
        body: function() {
            try {
                var propName = 1;
                Object.defineProperty(Array.prototype, propName, {
                    value: 321,
                    writable: true, configurable: true, enumerable: true
                });

                var o = helpers.getDummyObject();
                o[0] = 10;
                o.length = 3;
                o.sort = Array.prototype.sort;
                o.join = Array.prototype.join;
                o.toString = Array.prototype.toString;

                o.sort();
                assert.areEqual("10,321,", o.toString(), "sort result mismatch?");
            } finally {
                delete Array.prototype[propName];
            }
        }
    },

    test_322_i: {
      name: "Convert accessor to a data property for non-extensible object (WIN8 bug 463559) but for numeric property",
      body: function () {
        var o = helpers.getDummyObject();
        var propertyName = "1";

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
};  // tests.

var passes = {
    pass1: {
        name: "obj",
        prep: function() {
            helpers.getDummyObject = function() {
                return {}; // a normal object
            };
        }
    },

    pass2: {
        name: "arr",
        prep: function() {
            helpers.getDummyObject = function() {
                return []; // a normal array
            };
        }
    },

    pass3: {
        name: "es5arr",
        prep: function() {
            helpers.getDummyObject = function() {
                var arr = [];
                Object.defineProperty(arr, "12345", {
                    get: function() {
                        helpers.writeln("dummy called");
                    },
                    configurable: true
                });
                delete arr[12345];
                return arr; // an ES5 array
            };
        }
    },

    pass4: {
        name: "misc",
        runonce: true, // Run misc tests only once
        prep: function() {
            helpers.getDummyObject = function() {
                return null; // Misc tests do not use helpers.getDummyObject
            };
        }
    }

};

testRunner.runTests(passes, tests);
