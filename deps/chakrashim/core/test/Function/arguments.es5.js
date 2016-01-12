//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

if (typeof(WScript) != "undefined") {
  WScript.LoadScriptFile("..\\UnitTestFramework\\UnitTestFramework.js");
}

var tests = {
  // Note: each test has name (string) and body (function) properties.
  //       Success is when the body does not throw, failure -- when it throws.

  test01: {
    name: "formal arg: simple: verify connection: named vs indexed arg",
    body: function () {
      var passedValue = 1;
      function f(a) {
        var val1 = 2;
        a = val1;
        assert.areEqual(val1, a, "wrong value of named parameter (val1)");
        assert.areEqual(val1, arguments[0], "wrong value of indexed parameter (val1)");

        var val2 = 3;
        arguments[0] = val2;
        assert.areEqual(val2, arguments[0], "wrong value of indexed parameter (val2)");
        assert.areEqual(val2, a, "wrong value of named parameter (val2)");
      }
      f(passedValue);
    }
  },

  test02: {
    name: "formal arg: defineProperty, check property descriptor",
    body: function () {
      var passedValue = 1;
      function f(a) {
        var val = 2;
        Object.defineProperty(arguments, 0, { configurable: false, enumerable: false, value: val });
        // Note that we expect writable: true because this was omitted in defineProperty above
        // which is acually re-define property with all attributes == true.
        var expected = { configurable: false, enumerable: false, writable: true, value: val };
        assert.areEqual(expected, Object.getOwnPropertyDescriptor(arguments, 0), "wrong value of getOwnPropertyDescriptor");
        assert.areEqual(val, a, "wrong value of named parameter");
      }
      f(passedValue);
    }
  },

  test03: {
    name: "formal arg: defineProperty, set writable to false, verify writability and lost connection. WOOB 1128023",
    body: function () {
      var passedValue = 1;
      function f(a) {
        Object.defineProperty(arguments, 0, { writable: false });
        var expected = { configurable: true, enumerable: true, writable: false, value: passedValue };
        assert.areEqual(expected, Object.getOwnPropertyDescriptor(arguments, 0), "wrong value of getOwnPropertyDescriptor");

        // Attempt to change arguments[0] which is not writable now.
        var val1 = 2;
        arguments[0] = val1;
        assert.areEqual(passedValue, arguments[0], "non-writable changed");
        assert.areEqual(passedValue, a, "non-writable changed: named arg also changed");

        // Change named arg value, verify we are in connection named vs indexed arg.
        var val2 = 3;
        a = val2;
        assert.areEqual(val2, a, "Attemp to change named arg: didn't work");
        assert.areEqual(passedValue, arguments[0], "At this time we should not be connected, but we are");
      }
      f(passedValue);
    }
  },

  test04: {
    name: "formal arg: defineProperty, set writable to false AND set value, verify that value changed in both named and indexed arg and that the item was disconnected",
    body: function () {
      var passedValue = 1;
      function f(a) {
        var val1 = 2;
        var val2 = 3;
        Object.defineProperty(arguments, 0, { writable: false, value: val1 });
        var expected = { configurable: true, enumerable: true, writable: false, value: val1 };
        assert.areEqual(expected, Object.getOwnPropertyDescriptor(arguments, 0), "wrong value of getOwnPropertyDescriptor");

        assert.areEqual(val1, arguments[0], "value: arguments[0]");
        assert.areEqual(val1, a, "value: a");

        // Verify we are disconnected now.

        a = val2;
        assert.areEqual(val2, a, "new value: a");
        assert.areEqual(val1, arguments[0], "value: arguments[0] -- did not get disconnected!");
      }
      f(passedValue);
    }
  },

  test05: {
    name: "formal arg: defineProperty w/o cause of disconnect, verify still connected to named arg",
    body: function () {
      var passedValue = 1;
      var val1 = 2;
      var val2 = 3;
      function f(a) {
        Object.defineProperty(arguments, 0, { value: val1 });
        a = val1;
        assert.areEqual(val1, arguments[0], "arguments[0] got disconnected");
        arguments[0] = val2;
        assert.areEqual(val2, a, "a got disconnected");
      }
      f(passedValue);
    }
  },

  test06: {
    name: "formal arg: defineProperty, disconnect arg[0], verify that arg[1] is still connected",
    body: function () {
      function f(a, b) {
        Object.defineProperty(arguments, 0, { writable: false });
        var val1 = 3;
        var val2 = 4;
        arguments[1] = val1;
        assert.areEqual(val1, b, "arg[1] got disconnected");
        b = val2;
        assert.areEqual(val2, arguments[1], "arg[1] got disconnected");
      }
      f(1, 2);
    }
  },

  test07: {
    name: "formal arg: defineProperty: convert to accessor property",
    body: function () {
      function f(a) {
        var isGetterFired = false;
        var isSetterFired = false;

        Object.defineProperty(arguments, 0, {
          get: function() { isGetterFired = true; return this.value; },
          set: function(arg) { isSetterFired = true; this.value = arg; }
        });

        assert.areEqual(undefined, arguments[0], "unexpected arg[0] value right after conversion to accessor property");
        assert.areEqual(true, isGetterFired, "isGetterFired (1)");
        isGetterFired = false;

        var val1 = 2;
        arguments[0] = val1;
        assert.areEqual(true, isSetterFired, "isSetterFired");
        assert.areEqual(val1, arguments[0], "get value after set");
        assert.areEqual(true, isGetterFired, "isGetterFired (2)");
      }
      f(1);
    }
  },

  test08: {
    name: "formal arg: defineProperty: convert to accessor, then to data property, verify value and that connection is lost",
    body: function () {
      var passedValue = 1;
      var val1 = 2;
      var val2 = 3;
      function f(a) {
        Object.defineProperty(arguments, 0, {
          get: function() { return this.value; },
          set: function(arg) { this.value = arg; }
        });
        Object.defineProperty(arguments, 0, { value: val1 });
        a = val2;

        assert.areEqual(arguments[0], val1, "arguments[0]");
        assert.areNotEqual(arguments[0], a, "arguments[0] != a");
      }
      f(passedValue);
    }
  },

  test09: {
    name: "formal arg: defineProperty: force convert to ES5 version but keep connected, check enumeration",
    body: function () {
      var passedValue = 1;
      function f(a) {
        Object.defineProperty(arguments, 0, { enumerable: true });
        var accumulator = "";
        for (var i in arguments) {
          accumulator += i.toString() + ": " + arguments[i] + ";";
        }
        assert.areEqual("0: " + passedValue + ";" , accumulator, "accumulator");
      }
      f(passedValue);
    }
  },

  test10: {
    name: "formal arg: defineProperty: set non-enumerable/non-writable/delete, check enumeration",
    body: function () {
      var passedValue1 = 2;
      var passedValue2 = 4;
      function f(a, b, c, d) {
        Object.defineProperty(arguments, 0, { enumerable: false }); // arguments[0].enumerable = false.
        Object.defineProperty(arguments, 1, { writable: false });   // arguments[1].writable = false -> disconnected.
        delete arguments[2];                                            // arguments[2] is deleted.
        var i, accumulator = "";
        for (i in arguments) {
          accumulator += i.toString() + ": " + arguments[i] + ";";
        }
        // Note that we expect [1].enumerable = true because this was omitted in defineProperty above
        // which is acually re-define property that previously already had enumerable = true.
        assert.areEqual("1: " + passedValue1 + ";" + "3: " + passedValue2 + ";", accumulator, "accumulator");
      }
      f(1, passedValue1, 3, passedValue2);
    }
  },

  test11: {
    name: "passed/undeclared arg: verify there is no correlation with Object.prototype indexed data properties. WOOB 1143896",
    body: function () {
      var passedValue = "passed";
      Object.defineProperty(Object.prototype, 0, { value: "from proto", configurable: true, writable: false });
      try {
        function f() { return arguments; }
        var argObj = f(passedValue);
        assert.areEqual(passedValue, argObj[0]);
      } finally {
        delete Object.prototype[0];
      }
    }
  },

  test12: {
    name: "formal arg: verify there is no correlation with Object.prototype indexed properties",
    body: function () {
      var passedValue = "passed";
      Object.defineProperty(Object.prototype, 0, { value: "from proto", configurable: true, writable: false });
      try {
        function f(a) { return arguments }
        var argObj = f(passedValue);
        assert.areEqual(passedValue, argObj[0]);
      } finally {
        delete Object.prototype[0];
      }
    }
  },

  test13: {
    name: "passed/undeclared arg: verify there is no correlation with Object.prototype indexed accessor properties. WOOB 1144602",
    body: function () {
      var initial = "initial";
      var passedValue = "passed";
      var data = initial;
      Object.defineProperty(Object.prototype, 0, {
        configurable: true,
        get: function() { return data; },
        set: function(arg) { data = arg; }
      });
      try {
        function f() { return arguments; }
        var argObj = f(passedValue);
        assert.areEqual(initial, data, "data: should not be changed as setter on prototype should not be fired");
        assert.areEqual(passedValue, argObj[0], "argObj[0]");
      } finally {
        delete Object.prototype[0];
      }
    }
  },

  test14: {
    name: "formal arg: verify there is no correlation with Object.prototype indexed accessor properties",
    body: function () {
      var initial = "initial";
      var passedValue = "passed";
      var data = initial;
      Object.defineProperty(Object.prototype, 0, {
        configurable: true,
        get: function() { return data; },
        set: function(arg) { data = arg; }
      });
      try {
        function f(a) { return arguments; }
        var argObj = f(passedValue);
        assert.areEqual(initial, data, "data: should not be changed as setter on prototype should not be fired");
        assert.areEqual(passedValue, argObj[0], "argObj[0]");
      } finally {
        delete Object.prototype[0];
      }
    }
  },

  test15: {
    name: "formal arg: delete, make sure it's deleted",
    body: function () {
      var passedValue = 1;
      function f(a) {
        Object.defineProperty(arguments, 0, { enumerable: false }); // Force convert to ES5 version.
        delete arguments[0];
        assert.areEqual(undefined, arguments[0], "was not deleted.");
        assert.areEqual(passedValue, a, "a is changed.");
      }
      f(passedValue);
    }
  },

  test16: {
    name: "formal arg: delete, add, check named arg is not changed",
    body: function () {
      var passedValue = 1;
      function f(a, b) {
        Object.defineProperty(arguments, 0, { enumerable: false }); // Force convert to ES5 version.
        delete arguments[0];
        arguments[0] = passedValue + 1;
        assert.areEqual(passedValue, a, "a is changed.");
      }
      f(passedValue, 2);
    }
  },

  test17: {
    name: "formal arg: delete, then defineProperty with attributes for data property, check the value",
    body: function () {
      var passedValue = 1;
      function f(a) {
        delete arguments[0];
        var val = 2;
        Object.defineProperty(arguments, 0, { enumerable: true, configurable: true, writable: true, value: val });
        assert.areEqual(val, arguments[0], "wrong value");
      }
      f(passedValue);
    }
  },

  test18: {
    name: "formal arg: delete, then defineProperty with attributes for accessor property, check the enumeration",
    body: function () {
      var passedValue = 1;
      var getter = function() {return this.value; };
      var setter = function(arg) { this.value = arg; };
      function f(a) {
        delete arguments[0];
        Object.defineProperty(arguments, 0, { enumerable: true, configurable: true, get: getter, set: setter });
        var expected = { configurable: true, enumerable: true, get: getter, set: setter };
        assert.areEqual(expected, Object.getOwnPropertyDescriptor(arguments, 0), "wrong descriptor");
        var accumulator = "";
        for (var i in arguments) {
          accumulator += i.toString() + ": " + arguments[i] + ";";
        }
        assert.areEqual("0: " + undefined + ";", accumulator, "accumulator 2");
      }
      f(passedValue);
    }
  },

  test19: {
    name: "formal arg, es5 heap arguments: delete, add, check enumerable/order",
    body: function () {
      var passedValue1 = 1;
      var passedValue2 = 2;
      var newValue1 = 100;
      var newValue2 = 200;
      var i, accumulator;
      function f(a, b) {
        // Scenario 1: delete prior to converting to ES5 version.
        delete arguments[0];                                       // Delete [0] prior to conversion to ES5.
        Object.defineProperty(arguments, 0, { configurable: true, enumerable: true, value: newValue1 }); // Bring back [0] by defineProperty. Now args is ES5.
        accumulator = "";
        for (i in arguments) {
          accumulator += i.toString() + ": " + arguments[i] + ";";
        }
        assert.areEqual("0: " + newValue1 + ";" + "1: " + passedValue2 + ";", accumulator, "accumulator 1");

        // Scenario 2: delete after converting to ES5 version.
        Object.defineProperty(arguments, 0, { configurable: true, enumerable: true, writable: true, value: newValue1 }); // Bring back [0] by defineProperty. Now args is ES5.
        delete arguments[0];                                      // Delete [0] prior after conversion to ES5.
        arguments[0] = newValue2;                                 // Bring back [0] by setting value.
        accumulator = "";
        for (i in arguments) {
          accumulator += i.toString() + ": " + arguments[i] + ";";
        }
        assert.areEqual("0: " + newValue2 + ";" + "1: " + passedValue2 + ";", accumulator, "accumulator 2");
      }
      f(passedValue1, passedValue2);
    }
  },

  test20: {
    name: "formal arg, es5 heap arguments: delete, add, keep another arg in objectArray and use one non-formal, check enumerable/order",
    body: function () {
      var passedValue1 = 1;
      var passedValue2 = 2;
      var passedValue3 = 3;
      var passedValue4 = 4;
      var newValue = 100;

      function f(a, b, c) {
        Object.defineProperty(arguments, 0, { enumerable: true }); // Add objectArray item
        Object.defineProperty(arguments, 2, { enumerable: true }); // Add objectArray item
        var accumulator = "";
        delete arguments[0];
        arguments[0] = newValue;
        for (var i in arguments) {
          accumulator += i.toString() + ": " + arguments[i] + ";";
        }
        assert.areEqual(
          "0: " + newValue + ";" + "1: " + passedValue2 + ";" + "2: " + passedValue3 + ";" + "3: " + passedValue4 + ";",
          accumulator,
          "accumulator");
      }
      f(passedValue1, passedValue2, passedValue3, passedValue4);
    }
  },

  test21: {
    name: "formal arg: defineProperty, set enumerable to false, check getOwnPropertyNames",
    body: function (a, b) {
      function f(a) {
        Object.defineProperty(arguments, 0, { enumerable: false });
        // Note: Object.getOwnPropertyNames returns all properties, even non-enumerable.
        var actual = Object.getOwnPropertyNames(arguments);
        var expected = { 0: "0", 1: "1", 2: "length", 3: "callee" };
        assert.areEqual(expected, actual, "wrong property names");
      }
      f(101, 102);
    }
  },

  test22Helper: function test22Helper(isConvertNeeded, messagePrefix) {
    function mkerr(message) {
      return messagePrefix + ": " + message;
    }

    var passedValue = 1;
    var newPropertyName = "x";
    function f(a, b) {
      if (isConvertNeeded) {
        Object.defineProperty(arguments, 1, { enumerable: true }); // Force convert to ES5 version.
      }

      Object.preventExtensions(arguments); // No new properties can be added.
      assert.areEqual(false, Object.isExtensible(arguments), mkerr("isExtensible"));
      try {
        Object.defineProperty(arguments, newPropertyName, { enumerable: true, value: 100 }); // add new property
        assert.fail(mkerr("did not throw exception"));
      } catch (ex) {
      }

      arguments[newPropertyName] = 100;
      assert.areEqual(undefined, arguments[newPropertyName], mkerr("New property was added after preventExtensions was called"));
    }
    f(passedValue, passedValue + 1);
  },

  test22_1: {
    name: "arguments (non-ES5 version): call Object.preventExtensions, try add new property by defineProperty and direct set",
    body: function () {
      tests.test22Helper(false, "non-ES5 version");
    }
  },

  test22_2: {
    name: "arguments (ES5 version): call Object.preventExtensions, try add new property by defineProperty and direct set",
    body: function () {
      tests.test22Helper(true, "ES5 version");
    }
  },

  test23Helper: function test23Helper(isConvertNeeded, messagePrefix) {
    function mkerr(message) {
      return messagePrefix + ": " + message;
    }

    var passedValue = 1;
    function f(a, b) {
      if (isConvertNeeded) {
        Object.defineProperty(arguments, 1, { enumerable: true }); // Force convert to ES5 version.
      }

      Object.preventExtensions(arguments); // This causes configurable, writable = false for all properties + Object.preventExtensions.
      // Note: formals existed prior to calling Object.preventExtensions, thus they are still modifiable.
      assert.areEqual(false, Object.isExtensible(arguments), "isExtensible");

      var actual = Object.getOwnPropertyDescriptor(arguments, 0);
      var expected = { configurable: true, enumerable: true, writable: true, value: passedValue };
      assert.areEqual(expected, actual, mkerr("wrong descriptor - initial"));

      // Try to modify/re-configure
      // Note: do not change value here as it causes different code path than excersized by identified issue.
      Object.defineProperty(arguments, 0, { enumerable: false });
      Object.defineProperty(arguments, 0, { writable: false });
      Object.defineProperty(arguments, 0, { configurable: false });
      var expected = { configurable: false, enumerable: false, writable: false, value: passedValue };
      assert.areEqual(expected, Object.getOwnPropertyDescriptor(arguments, 0), mkerr("wrong descriptor - after redefine"));
    }
    f(passedValue, passedValue + 1);
  },

  // After Object.preventExtensions(arguments) we can't modify the attributes on formals.
  test23_1: {
    name: "arguments (non-ES5 version): call Object.preventExtensions, make sure we can still modify atttibutes on formals without changing the value",
    body: function () {
      tests.test23Helper(false, "non-ES5 version");
    }
  },

  // After Object.preventExtensions(arguments) we can't modify the attributes on formals.
  test23_2: {
    name: "arguments (ES5 version): call Object.preventExtensions, make sure we can still modify atttibutes on formals without changing the value",
    body: function () {
      tests.test23Helper(true, "ES5 version");
    }
  },

  test24Helper: function test24Helper(isConvertNeeded, messagePrefix) {
    function mkerr(message) {
      return messagePrefix + ": " + message;
    }

    var passedValue = 1;
    function f(a, b) {
      if (isConvertNeeded) {
        Object.defineProperty(arguments, 1, { enumerable: true }); // Force convert to ES5 version.
      }

      Object.seal(arguments); // This causes configurable = false for all properties + Object.preventExtensions.

      assert.areEqual(true, Object.isSealed(arguments), mkerr("isSealed"));
      assert.areEqual(false, Object.isExtensible(arguments), mkerr("isExtensible"));

      var actual = Object.getOwnPropertyDescriptor(arguments, 0);
      var expected = { configurable: false, enumerable: true, writable: true, value: passedValue };
      assert.areEqual(expected, actual, mkerr("wrong descriptor"));
    }
    f(passedValue, passedValue + 1);
  },

  // Object.freeze(arguments -- not ES5 version) does not set configurable to false on formals.
  test24_1: {
    name: "arguments (non-ES5 version): call Object.seal, verify desciptor on formal",
    body: function () {
      tests.test24Helper(false, "non-ES5 version");
    }
  },

  test24_2: {
    name: "arguments (ES5 version): call Object.seal, verify desciptor on formal",
    body: function () {
      tests.test24Helper(true, "ES5 version");
    }
  },

  test25Helper: function test25Helper(isConvertNeeded, messagePrefix) {
    function mkerr(message) {
      return messagePrefix + ": " + message;
    }

    var passedValue = 1;
    function f(a, b) {
      if (isConvertNeeded) {
        Object.defineProperty(arguments, 1, { enumerable: true }); // Force convert to ES5 version.
      }

      Object.freeze(arguments); // This causes configurable AND writable = false for all properties + Object.preventExtensions.

      assert.areEqual(true, Object.isFrozen(arguments), mkerr("isFrozen"));
      assert.areEqual(true, Object.isSealed(arguments), mkerr("isSealed"));
      assert.areEqual(false, Object.isExtensible(arguments), mkerr("isExtensible"));

      var actual = Object.getOwnPropertyDescriptor(arguments, 0);
      var expected = { configurable: false, enumerable: true, writable: false, value: passedValue };
      assert.areEqual(expected, actual, mkerr("wrong descriptor"));
    }
    f(passedValue, passedValue + 1);
  },

  // Object.freeze(arguments -- not ES5 version) does not set configurable and writable to false on formals.
  test25_1: {
    name: "arguments (non-ES5 version): call Object.freeze, verify descriptor on formal",
    body: function () {
      tests.test25Helper(false, "non-ES5 version");
    }
  },

  test25_2: {
    name: "arguments (ES5 version): call Object.freeze, verify descriptor on formal",
    body: function () {
      tests.test25Helper(true, "ES5 version");
    }
  },

  test26: {
    name: "formal arg: delete, preventExtensions, enumerate, make sure the item is deleted",
    body: function () {
      var passedValue1 = 1;
      var passedValue2 = 2;
      function f(a, b) {
        delete arguments[1];
        Object.preventExtensions(arguments);
        var accumulator = "";
        for (var i in arguments) {
          accumulator += i.toString() + ": " + arguments[i] + ";";
        }
        assert.areEqual("0: " + passedValue1 + ";", accumulator, "accumulator");
        assert.areEqual(undefined, arguments[1], "arguments[1]");
      }
      f(passedValue1, passedValue2);
    }
  },

  test27: {
    name: "formal arg: convert to ES5 version, change value and set writable to false",
    body: function () {
      var passedValue1 = 1;
      var val = 2;
      function f(a) {
        Object.defineProperty(arguments, 0, { enumerable: true });
        a = val;
        Object.defineProperty(arguments, 0, { writable: false });
        var expected = { configurable: true, enumerable: true, writable: false, value: val };
        assert.areEqual(expected, Object.getOwnPropertyDescriptor(arguments, 0));
      }
      f(passedValue1);
    }
  },

  test28: {
    name: "formal arg: convert to ES5 version, enumerate when number of actual params is less than number of formals",
    body: function () {
      var accumulator = "";
      function f(a, b) {
        Object.preventExtensions(arguments);
        for (var i in arguments) {
          if (accumulator.length != 0) accumulator += ",";
          accumulator += arguments[i];
        }
      }
      var value = 5;
      f(value);
      var expected = helpers.isVersion10OrLater ?
        value.toString() :
        value.toString() + ",undefined"; // IE9 compat mode -- Win8 558490.
      assert.areEqual(expected, accumulator, "Wrong accumulated value");
    }
  },

} // tests.

testRunner.runTests(tests);
