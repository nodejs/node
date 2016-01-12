//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

Object.prototype.prop0 = -4;

function test0(arg) {
  var obj1 = {};
  obj0 = obj1;
  obj1.prop1 += 0;

  if (arg) {
    obj1.prop1 = obj1.prop1.prop0;
  } else {
    obj1.prop1 = obj0.prop0;
    obj0.prop0 = obj1;
  }
}
test0(true);
test0(true);
test0(true);
WScript.Echo("PASSED");