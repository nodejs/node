//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function test0() {
  var loopInvariant = 11;
  var protoObj2 = {};
  var func1 = function () {
    var func5 = function () {
      function func6() {
      }
    };
  };
  var func3 = function () {
  };
  Object.prototype.method1 = func3;
  var ary = Array();
  var VarArr0 = [
      protoObj2,
      -333399140,
      65535
    ];
  VarArr0[VarArr0.length] = -829014994;
  VarArr0[6] = 1571438007.1;
  do {
    if (3 > loopInvariant) {
      break;
    }
    for (var _strvar1 in ary) {
      loopInvariant--;
      ary[_strvar1] = VarArr0.reverse();
    }
  } while ({});
}
test0();
test0();

WScript.Echo("pass");
