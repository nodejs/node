//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

if (typeof (WScript) != "undefined") {
  WScript.LoadScriptFile("..\\UnitTestFramework\\UnitTestFramework.js", "self");
}

var tests = {
  test01: {
    name: "RegExp.options property",
    body: function () {
      var re = /x/i;
      var desc = Object.getOwnPropertyDescriptor(re, "options");
      var expected = helpers.isCompatVersion9 ? 
        {enumerable: true, configurable: true, writable: true, value: "i"} :
        {enumerable: false, configurable: false, writable: false, value: "i"};
      assert.areEqual(expected, desc);
    }
  },
};

testRunner.run(tests);
