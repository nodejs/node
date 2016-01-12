//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

var shouldBailout = false;
var count = 0;

function test0(){
  var func1 = function(){
    h = d;
  }
  var d = 309820929.1;
  (((shouldBailout ? (d = { valueOf: function() { count++; return 3; } }, 1) : 1) ? +d : 1) );
};

// generate profile
test0();

// run JITted code
test0();

// run code with bailouts enabled
shouldBailout = true;
test0();

if (count == 1)
{
    WScript.Echo("Passed");
}
else
{
    WScript.Echo("FAILED");
}