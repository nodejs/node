//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

var shouldBailout = false;
function test0(){
  var obj0 = {};
  var obj1 = {};
  var func0 = function(){
  }
  var func1 = function(argObj0,argArr1,argMath2){
    eval("");
    func0();
  }
  Object.prototype.method0 = func1; 
  obj1.length = ((shouldBailout ? func0 = obj0.method0 : 1), func0()); 
};

var testOutcome = false;

try
{
// generate profile
test0(); 

// run JITted code
runningJITtedCode = true;
test0(); 

// run code with bailouts enabled
shouldBailout = true;
test0(); 
}
catch(e)
{
  WScript.Echo("Caught expected exception. Type of exception: " + e);
  if (e == "Error: Out of stack space") {
    testOutcome = true;
  }
}

if (testOutcome) {
  WScript.Echo("Passed");
}
else {
  WScript.Echo("Failed");
}
