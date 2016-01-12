//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

var isPassed1 = false;
var isPassed2 = false;
function test1() {
    try {
        for (var ubwtog in Object(Math.imul(1073741824, Object(Symbol())))) { }
    }
    catch (ex) {
        if (ex instanceof TypeError) {
            if (ex.message === 'Number expected') {
                isPassed1 = true;
            }
        }
    }
}

function test2() {
  ejdmhf_0 = new Uint8Array();
  try
  {
    ejdmhf_0[50341] = Symbol();
  }
  catch(ex)
  {
      if(ex instanceof TypeError) {
          if(ex.message === 'Number expected') {
              isPassed2 = true;
          }
      }
  }
}

test1();
test2();
test2();
test2();

WScript.Echo(isPassed1 ? 'PASS' : 'FAIL');
WScript.Echo(isPassed2 ? 'PASS' : 'FAIL');
