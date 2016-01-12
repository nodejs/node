//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

// Notes on running this script:
// - rldirs.xml is set up in the following way so that this script is called 2 times:
//   - First, it's called for interpreted variant.
//   - Then, when it's called for dynapogo variant, jshost is called with: -args dynapogo.
//   - This script is not called for the default variant.
// - Idea: 
//   - Collect dynamic profile cache when called for interpreted variant.
//   - Use dynamic profle cache when called with -args dynapogo.
//   - Some tests cause bailout by passing different parameter to test function 
//     as when dynamic profile cache was created.
// - How to manually run/repro:
//   - jshost -nonative -dynamicprofilecache:inlineBuiltIns.dpl inlineBuiltIns.js
//   - jshost -forcenative -dynamicprofileinput:inlineBuiltIns.dpl inlineBuiltIns.js -args dynapogo
//   - also trying using -testtrace:bailout  to make sure you get the bailouts.
// TODO: change passing -args native to jshost and instead 
//       add support to WScript to expose getFlagByString() for Js::ConfigFlagsTable flags and check for -native.

if (typeof (WScript) != "undefined") {
  WScript.LoadScriptFile("..\\UnitTestFramework\\UnitTestFramework.js", "self");
}

var Runtime = {
  // returns true when this jshost is called with arguments: -args native
  get isDynapogo() {
    return WScript.Arguments.length > 0 && WScript.Arguments[0] == "dynapogo";
  }
};

var tests = {
  test01: {
    name: "Bailout on function = null",
    body: function () {
      function TestSin(x) {
        var r1 = Math.sin(x);
        return r1;
      };
      if (Runtime.isDynapogo) {
        var sinOrig = Math.sin;  // back up Math.sin
        assert.throws(function() {
            Math.sin = null;
            var r = TestSin({});
            assert.isTrue(isNaN(r));
          }, TypeError);
        Math.sin = sinOrig;      // restore Math.sin
      }
      else TestSin();
    }
  },

  test02: {
    name: "Bailout on function = object, not a function",
    body: function () {
      function TestSin(x) {
        var r1 = Math.sin(x);
        return r1;
      };
      if (Runtime.isDynapogo) {
        var sinOrig = Math.sin;  // back up Math.sin
        assert.throws(function() {
            Math.sin = {};
            var r = TestSin({});
            assert.isTrue(isNaN(r));
          }, TypeError);
        Math.sin = sinOrig;      // restore Math.sin
      }
      else TestSin();
    }
  },

  test03: {
    name: "Bailout on function = wrong function",
    body: function () {
      function TestSin(x) {
        var r1 = Math.sin(x);
        return r1;
      };
      var sinOrig = Math.sin;  // back up Math.sin
      Math.sin = Math.cos;
      var r = TestSin({});
      assert.isTrue(isNaN(r));
      Math.sin = sinOrig;      // restore Math.sin
    }
  },

  test04: {
    name: "Bailout on argument = string",
    body: function () {
      function TestSin(x) {
        var r1 = Math.sin(x);
        return r1;
      };
      var r = TestSin("string");
      assert.isTrue(isNaN(r));
    }
  },

  test05: {
    name: "Bailout on argument = object",
    body: function () {
      function TestSin(x) {
        var r1 = Math.sin(x);
        return r1;
      };
      var r = TestSin({});
      assert.isTrue(isNaN(r));
    }
  },

  test06: {
    name: "Bailout on 2nd argument = string",
    body: function () {
      function TestPow(x, y) {
        var r1 = Math.pow(x, y);
        return r1;
      };
      var r = TestPow(2, "string");
      assert.isTrue(isNaN(r));
    }
  },

  test07: {
    name: "Float/int type specialized argOuts which we restore at bailout",
    body: function () {
      // As long as there is no assert/crash, we are fine.
      (function() {
        var i = -8.1E+18;
        var r = Math.pow(1, Math.exp(Math.atan2(1, ((~i) - 737882964))));
      })();

      (function() {
        var e = 1;
        return Math.pow(e >> 1, 3.2)
      })();

      (function() {
        var e = 1;
        Math.atan2(1, Math.pow((e >>= 1), Math.tan((-1031883772 * Math.abs(-951135089)))));
      })();

      (function() {
        var ary = new Array();
        ary[0] = 0;
        Math.pow(1808815940.1, -ary[0]);
      })();

      (function() {
        return Math.pow(Math.sin(1), Math.pow(1, 1));
      })();

      (function() { 
        var o = { x: 0 };
        var func0 = function()
        {
          Math.pow(1.1, o.x * -1);
        }
        Math.atan2(func0(), 1);
      })();
    }
  },

  test08: {
    name: "Bailout on argument after function copy-prop into InlineBuiltInStart",
    body: function () {
      for(var i in [0, 1])
        assert.isTrue(isNaN(Math.pow(Math.pow(/x/, 0.1), 0.1)));
    }
  },

  test09: {
    name: "Bailout (pre-op) on 2nd arg which is updated in the place of the call - make sure 1st arg is not updated",
    body: function() {
      var accumulator = "";
      var vOf = function vOf() { accumulator += "x"; return 3; }
      function testFunc() {
          var i = 1;
          do {
              // We need to make sure that we pass original value of i (== 1) as 1st arg.
              var x = Math.pow(i, Runtime.isDynapogo ? i = { valueOf: vOf } : 1);
          } while (vOf == undefined);
      }
      testFunc();
      if (Runtime.isDynapogo) {
        assert.areEqual("x", accumulator, "valueOf was called wrong number of times");
      }
    }
  },
}; // tests.

testRunner.runTests(tests);
