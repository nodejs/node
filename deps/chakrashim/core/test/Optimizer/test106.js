//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------


function FAILED()
{
    WScript.Echo("FAILED");
    throw(1);
}

function test0(){
  var arrObj0 = {};
  if((2 % 2)) {
  }
  else {
    b =(2 & 2);
  }
  arrObj0.length =b;
  if (arrObj0.length != 2)
    FAILED();
};

// generate profile
test0();
test0();
test0();

// run JITted code
runningJITtedCode = true;
test0();
test0();
test0();

// run code with bailouts enabled
shouldBailout = true;
test0();


WScript.Echo("Passed");