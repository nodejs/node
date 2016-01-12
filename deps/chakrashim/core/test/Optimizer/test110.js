//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function test0(){
  eval("");
  var i32 = new Int32Array(256);
  var __loopvar1 = 0;
  for(var strvar0 in i32 ) {
    if(strvar0.indexOf('method') != -1) continue;
    if(__loopvar1++ > 3) break;
    i32[strvar0] = (-684194670.9 * 1); 
  }
};
test0();
test0();

WScript.Echo("pass");
