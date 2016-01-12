//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function test0(){
  var obj0 = {};
  var arrObj0 = {};
  var func0 = function(){
    eval("");
  }
  var func1 = function(){
    var obj4 = {nd0: {nd0: {lf0: {prop0: -46, prop1: 3, prop2: -2147483648, length: -6.02625054824609E+18 , method0: func0}}}};
    d ^=obj4.nd0.nd0.lf0.method0();
    obj4.nd0.nd0.lf0 = 1;
    this.prop1 |=obj4.nd0.nd0.lf0.method0.call(obj0 );
  }
  Object.prototype.method0 = func1; 
  var d = 1;
  arrObj0.method0();
};

var testOutcome = false;

try
{
// generate profile
test0(); 
test0(); 
test0(); 

// run JITted code
runningJITtedCode = true;
test0(); 
test0(); 
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
