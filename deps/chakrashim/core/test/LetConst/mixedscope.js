//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

let level1Func_1 = function level_1_identifier_0() {   
   const a= 1;  
   function level2Func() {
      eval("print(a)");
   }
   level2Func();
}
level1Func_1();

var level1Func_2 = function level_1_identifier_0() {   
    let level_1_identifier_2= "level1";    
          

    function level2Func() {        
           print(typeof level_1_identifier_0);
           level_1_identifier_2 += "level2";
    }
    level2Func();     
    print(level_1_identifier_2);

}
level1Func_2();

function a() {
    const x = 'x';
    const y = 'y';
    function b() {
        return true ? 0 : x
    }
    function c() {
        return false ? y : 1
    }
    print(b());
    print(null);
    print(c());
    print(null);
}
a();

function print(x) { WScript.Echo(x); }

