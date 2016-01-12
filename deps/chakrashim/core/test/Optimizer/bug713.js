//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

var shouldBailout = false;
var G = 0;

function test0(){
  var obj0 = {};
  var obj1 = {};
  var func1 = function(){
    var __loopvar2 = 0;
    while(__loopvar2 < 3) {
        __loopvar2++;
      while(a < (1)) {
        break ;
      }
      var a = 1;
      (shouldBailout ? (a = { valueOf: function () { G += 1; return 3; } }, 1) : 1);
    }
  }
  var func2 = function(){
  }
  obj0.method0 = func2;
  var i16 = new Int16Array(256);
  var ui8 = new Uint8Array(256);
  var a = 1;
  var c = 1;
  var d = 1;
  var e = 1;
  //Snippet 1: basic inlining test
  obj0.prop0 = (function(x,y,z) {
    obj1.prop0 = func1();

    return obj0.method0();
  })((c *= (shouldBailout ? (a = { valueOf: function() { G += 10; return 3; } }, ui8[((obj1.length, 2, 2.7970894295654E+18)) & 255]) : ui8[((obj1.length, 2, 2.7970894295654E+18)) & 255])),(d >>>= i16[((shouldBailout ? (a = { valueOf: function() { G += 100; return 3; } }, (! 2)) : (! 2))) & 255]),((~ 0) ^ a));
  
};

// generate profile
test0();

// run JITted code
test0();

// run code with bailouts enabled
shouldBailout = true;
test0();


if (G == 102)
{
    WScript.Echo("Passed");
}
else
{
    WScript.Echo("FAILED");
}
