//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

WScript.LoadScriptFile("..\\UnitTestFramework\\UnitTestFramework.js");
var x = WScript.LoadScriptFile("crossContext_remoteContext.js", "samethread");

var tests = {
  test01: {
    name: "Make sure when called with -nonative, marshaling of results is correct (Win8 628808)",
    body: function() {
      // Ñall this with -nonative
      // Win8 628808: the following cases used to cause an assertion.
      var str = "this is a sting";

      var result = str.match(x.re);
      var result = str.replace(x.re, null);
      var result = str.split(x.re, 1);
      var result = str.search(x.re);

      var result = x.str.match(x.re);
      var result = x.str.replace(x.re, null);
      var result = x.str.split(x.re, 1);
      var result = x.str.search(x.re);

      var result = x.strObject.match(x.re);
      var result = x.strObject.replace(x.re, null);
      var result = x.strObject.split(x.re, 1);
      var result = x.strObject.search(x.re);
      var result = String.prototype.replace.call(x.strObject, /forceNoMatch/, "");

      // The following cases are not impacted by Win8 628808, but it's worth verifying them for regressions in RegexHelper
      var result = x.str.replace(x.str, "I");
      var result = x.re.exec(x.str);
      var result = x.str.split(x.str, 1);

      var result = x.strObject.replace(x.strObject, "I");
      var result = x.re.exec(x.strObject);
      var result = x.strObject.split(x.strObject, 1);
    }
  },

  test02: {
    name: "lastIndex behavior",
    body: function() {
      x.reg.exec("_this_");
      assert.areEqual(5, x.reg.lastIndex, "wrong x.reg.lastIndex");
    }
  },

  test03: {
    name: "Updating $1, $2,.. behavior",
    body: function() {
      // Disabled for IE9-compat mode due to Win8 xxxxxxx.
      // TODO: re-enable when the bug is fixed.
      if (helpers.isVersion10OrLater) {
        "this".match(x.rep);
        assert.areEqual("t", RegExp.$1, "RegExp.$1 in local context wasn't updated to the capture group");
      }
    }
  },

  test04: {
    name: "Check in which context the results are created",
    body: function() {
      var result = "this".match(x.re);
      var expected = helpers.isVersion10OrLater ? Array : x.Array;
      assert.areEqual(expected, result.constructor, "The result should be created in local context");
    }
  },
}

testRunner.run(tests);
