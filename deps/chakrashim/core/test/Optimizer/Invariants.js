//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

// Everything inside the loop except for the assignment to 'a' should get hoisted
function test0() {
    var a;
    for(var i = 0; i < 1; ++i)
        a = (0x40000000 | 0) % 3;
    return a;
}
WScript.Echo("test0: " + test0());

// The '-f' bails out, and the Neg is hoisted outside the loop. The multiplication is not type-specialized, so 'f' is converted
// to var and that conversion is also hoisted outside the loop. The conversion to var happens after the bailout, so the value of
// the var sym for 'f' is not valid at the time of bailout. So, bailout should use the int sym for 'f' to restore its value.
function test1() {
    var c = 1;
    var f = (1 !== 0);
    f = f & 21037030;
    var g;
    for(var __loopvar1 = 0; c < (g = (((-f) ? (f * i32[(1) % 256]) : 1))) && __loopvar1 < 3; c++ + __loopvar1++) {
    }
    return g;
}
WScript.Echo("test1: " + test1());

// In 'o.p &= 1', 'o' is converted to var. 'o' was const-propped with '0' though, so an LdC_A_I4 is created and hoisted to the
// outer loop's landing pad. LdC_A_I4 should be considered a type-spec conversion here, so while making the var version of the
// sym live, it should also preserve the int version of the sym as live.
function test2() {
    for(var i = 0; i < 1; ++i) {
        var o = 0;
        for(var j = 0; j < 1; ++j)
            o.p &= 1;
    }
}
WScript.Echo("test2: " + test2());

// When hoisting an invariant with a new dst, value type of the old dst should be copied over to the new dst.
function test3() {
  var func1 = function () {
    return '6' + '\xb5!%$' + 'caller';
  };
  var func2 = function () {
     return '6' + '\xb5!%$' + 'caller';
  };
  
  var ary = Array();
  func1();
  for (var v1 = 0; v1 < 8; v1++) {
    WScript.Echo(func2());
  }
  WScript.Echo('subset_of_ary = ' + ary.slice());
}
test3();
