//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

// Configuration: ..\fuzz.xml
// Testcase Number: 1664
// Bailout Testing: ON
// -maxinterpretcount:1  -off:floattypespec
// Branch:  fbl_ie_dev1(saagarwa.
// Build: 20529-1700)

var Failed = 0;

function FAILED()
{
    Failed++;
    WScript.Echo("FAILED");
}

function test0(){
  var obj0 = {};
  var func1 = function(){
    (obj0.prop0 /=NaN);
    return Object.create(obj0);
  }

  obj0.prop0 = 1;
  obj0.prop1 = {prop2: ( Object.defineProperty(obj0, 'prop0', {writable: false}) )};
  (new func1()).prop0;
  if (obj0.prop0 !== 1) FAILED();
  func1();
  if (obj0.prop0 !== 1) FAILED();
};
// generate profile
test0();

// run JITted code
test0();

test0();

if (!Failed)
{
    WScript.Echo("Passed");
}
