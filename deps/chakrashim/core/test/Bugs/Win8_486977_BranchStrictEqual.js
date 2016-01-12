//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

// Win8 bug 486977
(function () {
  var a = [[1,2,3],[4,5,6]];
  var b = [[1,2,3],[4,5,6]];

  var testName = "a.length === b.length";
  if (a.length === b.length) WScript.Echo(testName, ": True"); else WScript.Echo(testName, ": False");

  var testName = "a.length !== b.length";
  if (a.length !== b.length) WScript.Echo(testName, ": True"); else WScript.Echo(testName, ": False");
})();
