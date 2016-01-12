//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

(function module(a){
  var func1 = function func1(p0) {};
  var c = 1 / Math.abs(func1(1, 1, 1, 1));

  // At this point during lowerer, we used to call (arm, chk build) LowererMD::SaveDoubleToVar, dstIsTemp=true 
  // and then further, call LegalizerMD::LegalizaSymOffset. Problem was that it can be called only after lowerer. 
  var xx0 = Math.abs(-1335766489); 

  var xx1 = 1 - xx0;
  var xx = 1 * xx1 * 1 + 1;
  WScript.Echo(xx);

  var d = 1;
  var e = 1;
  var ary = 1;
  var a = 1;
  var b = 1;
  var obj0 = new Object();
  var obj1 = new Object();
  var func0 = function(){
    d -=1;
  }
  var func2 = function(p0){};
  f = 1;
  var g = Math.log(xx);
  ((1 * func2() + ((1 * (func2() - 1)) - ((1 * (c - 1)) * ((g-- ) - (((obj1.length % 1) * 
    ((b %= 0) - (1 * (g + 1)))) * ((-- a) * 1 + Math.exp(1)) + ((495082858.1) + 1)))))));
  WScript.Echo("d = " + (d|0));;
  WScript.Echo("f = " + (f|0));;
  WScript.Echo("g = " + (g|0));;
  WScript.Echo("obj0 = " + (obj0|0));;
  WScript.Echo("obj1 = " + (obj1|0));;
  WScript.Echo("obj0.a = " + (obj0.a|0));;
  WScript.Echo("obj0.b = " + (obj0.b|0));;
  WScript.Echo("obj0.length = " + (obj0.length|0));;
  WScript.Echo("obj1.a = " + (obj1.a|0));;
  WScript.Echo("obj1.b = " + (obj1.b|0));;
  WScript.Echo("obj1.length = " + (obj1.length|0));;
  WScript.Echo("ary[0] = " + (ary[0]|0));;
  WScript.Echo("ary[1] = " + (ary[1]|0));;
  WScript.Echo("ary[100] = " + (ary[100]|0));;
  WScript.Echo("ary[ary.length-1] = " + (ary[ary.length-1]|0));;
  WScript.Echo("ary.length = " + (ary.length|0));;
})(5);
