//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function test0(){
  var obj1 = {};
  var arrObj0 = {};
  var ui8 = new Uint8Array(256);
  var intary = 1;
  var d = 1;
  var __loopvar1 = 0;
  for(var strvar0 in ui8 ) {
    if(strvar0.indexOf('method') != -1) continue;
    if(__loopvar1++ > 3) break;
    obj1.length =d;
    d =(1 & 2147483647);
    arrObj0.prop0 = intary[(((1 >= 0 ? 1 : 0)) & 0XF)]; 
    e =(+ d);
  }
};
test0(); 
test0(); 

WScript.Echo("pass");
