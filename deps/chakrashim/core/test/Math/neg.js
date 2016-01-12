//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function test0(){
    var x1 = -(1 << 31);
    var x2 = -(0x80000000 | 0);
    var x3 = -(0x80000000 + 0);
    var x4 = -(0x80000000);
    WScript.Echo("x1 = " + x1 + "   x2 = " + x2 + "   x3 = " + x3 + "   x4 = " + x4);
};

// generate profile
test0();

function test1(){
  var negate = function(p0){
    for(var i=0; i < 2; i++ ) {
      p0 =(- p0);
      WScript.Echo(p0);
    }
  }
  negate(-2147483648);
  negate(-5);
  negate(-1073741824);
};

test1();
