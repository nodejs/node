//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

// -loopinterpretcount:1 -bgjit- -force:fieldhoist -mic:1 -msjrc:1

var obj1 = {};
var arrObj0 = {};
var func2 = function (argObj5, argObj6) {
  do {
    argObj6.prop0 += protoObj1;
  } while (argObj5.length);
};
var func3 = function () {
};
obj1.method0 = func3;
obj1.method1 = func2;
protoObj1 = Object(obj1);
obj1.prop0 = 1;
var __loopvar0 = 7 - 13;
do {
  __loopvar0 += 4;
  if (__loopvar0 >= 7) {
    break;
  }
  protoObj1.method0(arrObj0, obj1.prop0, arrObj0);
  Object.defineProperty(obj1, 'prop0', {
    set: function () {
    }
  });
} while (2);

var v30 = obj1.method1(1,protoObj1);
var v30 = obj1.method1(1,protoObj1);

WScript.Echo("passed");
