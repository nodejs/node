//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

var pass = true;
function test() {
  var c = function () {};
  
  function shapeyConstructor() 
  {
    for (var x in Array(c = '', ...NaN, c = '')) {}
  }
  
    try 
    {
      var f = shapeyConstructor();
    } catch (e) {}
  if(typeof c !== "string")
  {
   print("FAIL: type of is "+typeof c);
   pass = false;
  }
}
test();
test();
test();
pass ? print("PASS") : print("FAIL");