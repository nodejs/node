//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

var __counter = 0;
function test0() {
  __counter++;
  function leaf() {
  }
  var obj0 = {};
  var obj1 = {};
  var arrObj0 = {};
  var func0 = function () {
    for (var _strvar0 in obj0) {
      if (_strvar0.indexOf('method') != -1) {
        continue;
      }
      return leaf();
    }
    return leaf();
    do {
    } while (arrObj0);
  };
  var func2 = function () {
  };
  obj0.method0 = func2;
  obj0.method1 = func0;
  arrObj0.method1 = func2;
  Object.prototype.prop0 = -21449704;
  var uniqobj27 = [
      obj1,
      obj0,
      arrObj0
    ];
  var uniqobj28 = uniqobj27[__counter];
  uniqobj28.method1();
}
try
{
    test0();
    test0();
    test0();
}
catch(e)
{
    WScript.Echo("PASS");
}
