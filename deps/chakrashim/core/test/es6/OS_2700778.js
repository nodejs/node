//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

var obj0 = {};
var obj1 = {};
var func1 = function () {
};
var func3 = function () {
  (function (argMath9, ...argArr10) {
    argMath9 = ~-1457942942;
    eval('');
  })();
};
obj0.method1 = func3;
obj1.method0 = obj0.method1;
protoObj1 = Object(obj1);
do {
  var uniqobj0 = [protoObj1];
  uniqobj0[0].method0();
  var uniqobj1 = [protoObj1];
  var uniqobj2 = uniqobj1[0];
  uniqobj2.method0();
  func1(obj0.method1());
} while (obj0.method1());
WScript.Echo("PASS");