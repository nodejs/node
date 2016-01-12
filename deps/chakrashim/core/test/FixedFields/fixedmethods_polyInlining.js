//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

var shouldBailout = false;
var runningJITtedCode = false;
var __counter = 0;
function test0() {
  var GiantPrintArray = [];
  __counter;
  function makeArrayLength(x) {
    {
      return 100;
    }
  }
  function leaf() {
  }
  var obj0 = {};
  var protoObj0 = {};
  var arrObj0 = {};
  var litObj0 = { prop1: 3.14159265358979 };
  var func0 = function () {
    typeof 985562408 != 'undefined';
    CollectGarbage();
    CollectGarbage();
  };
  var func1 = function (argFunc0, argFunc1) {
    //WScript.Echo("here");
    function v0() {
    }
    v0.prototype.method0 = function () {
    };
    v0.prototype.method1 = function () {
    };
    var v1 = new v0();
    NaN = func0.call(protoObj0);
    function v2() {
    }
    v2.prototype.p;
    v2.prototype.prop0;
    v2.prototype.method0 = function () {
    };
    (function () {
    });
    var v3 = new v2();
    function v4() {
    }
    v4.method0 = function () {
      NaN = obj0.length = +test0.caller;
    };
    v2.prototype = v4;
    var v5 = new v2();
    obj0;
    function v6() {
      this.method0 = function () {
        obj0.prop3 = argFunc0.call(obj0) + test0.caller;
      };
    }
    function v7() {
    }
    v6.prototype = v0.prototype;
    v7.prototype = new v6();
    var v8 = new v7();
    function v9() {
    }
    v9.prototype.method1 = function () {
    };
    v9.prototype = new v6();
    var v10 = new v9();
    function v11(v12) {
      // WScript.Echo(".");
      v12.method0();
    }

    v11(v3);
    v11(v5);
    v11(v3);
    v11(v10);
    v11(v1);
    v11(v8);
    v11(v10);

  };
  var func2 = function (argMath3, argFunc4, argMath5) {
    protoObj0.prop1 = ary.splice(6, 3, -937079010.9 * (-1491185136.9 + obj0.length) ^ ary.shift(), -2147483649 - (argMath3 >= obj0.length), protoObj0.length * (-937079010.9 * (-1491185136.9 + obj0.length) ^ ary.shift()) - (typeof 1766379515.1 == null), a === a || a != obj0.length, argMath5 - argMath5 | argMath5 === obj0.length, ~test0.caller, argFunc4.call(obj0), argFunc4.call(obj0) & 1034225882, -argFunc4.call(protoObj0), ary.push(-29553868 ^ 2012447008, -1114318962 + -1985634397, argMath5 - -926420530, -981986144, argMath5 - -926420530, h += argMath5, argMath5 - -926420530) >> ary.reverse()) >> argMath3;
  };
  var func3 = function (argMath7) {
    obj0.prop5 = (-476118889609086000 - ary.shift()) * test0.caller + (-argMath7 * (-1369803577084530000 + -1195616901.9) - 262279661);
    protoObj0.prop0 += -1369803577084530000;
    return func2.call(obj0, ~(1697877209799190000 - -1104416353.9), leaf, --obj0.length);
  };
  var func4 = function () {
    return ary.shift();
  };
  obj0.method0 = func2;
  obj0.method1 = func4;
  arrObj0.method0 = func1;
  arrObj0.method1 = func0;
  var ary = Array();
  var IntArr0 = Array();
  var FloatArr0 = Array(-701848834, 234290815);
  var VarArr0 = [];
  var a = -7471863111945560000;
  var b = 1034225882;
  var c = 44;
  var e = 2;
  var g = NaN;
  var h = 356353422;
  var i = 852783735;
  var j = 925107091;
  var k = 635272801;
  var m = -778124320;
  var n = -8065680838443580000;
  var q = 39;
  var r = 1697877209799190000;
  arrObj0[0] = 598806000;
  arrObj0[arrObj0] = -135;
  arrObj0.length = makeArrayLength();
  makeArrayLength();
  obj0;
  makeArrayLength();
  makeArrayLength();
  arrObj0.prop0 = 4294967297;
  prop3 = -1546252572.9;
  arguments;
  func2.call(obj0, ~(1697877209799190000 - -1104416353.9), leaf);
  m += typeof 985562408 != 'undefined';
  CollectGarbage();
  // CollectGarbage();
  ary.push(Object.create(arrObj0), obj0.method0.call(protoObj0,  func4(), leaf), arrObj0.method1());
  func3({
    prop8: 2,
    prop7: -5729096429004850000,
    prop6: 260823401.1,
    prop5: 1697877209799190000,
    prop4: 3694998724308620000,
    prop3: 635272801,
    prop1: 635272801,
    prop0: 1697877209799190000,
    44: -90469961
  });
  func1.call(litObj0, leaf, leaf);
  //WScript.Echo("after first");
  //var uniqobj0 = [obj0];
  //uniqobj0[__counter % uniqobj0.length].method1();
  //protoObj0;
  arrObj0.method0.call(litObj0, leaf, leaf, FloatArr0) * func0() + arrObj0.method0.call(litObj0, leaf);
  //WScript.Echo("after second");
  WScript(ary()(function () {
  }));
}
try{
    test0();
}
catch(e){}
WScript.Echo("Passed");
