//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function test0(){
  var obj2 = {};
  var func1 = function(){
    function func5 (arg0) {
      this.prop0 = arg0;
    }
    obj7 = new func5( );
  };
  
  obj2.method1 =  func1;
  obj7 = 1;
  var __loopvar0 = 2;
  for(;;) {
    if (__loopvar0 == 3) break;
    __loopvar0++;
    obj2.method1(obj7 );
    
    obj7 ;
  }
};

test0();
test0();

WScript.Echo("PASSED");
