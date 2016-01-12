//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

 function AsmModule(stdlib,foreign) {
    "use asm";

    var fun2 = foreign.fun2;

    
   

    function mul(x,y) {
        x = +x;
        y = +y;
        
        return (+(x*y));
    }

    
    function f2(x,y){
        x = +x;
        y = y|0;
        var i = 0, j = 0.0;
        j = +mul(+mul(x,1.),+mul(x,1.));
        return +j;
    }
    
    function f3(x){
        x = x|0;
        var i = 0.
        i = +f2(100.5,1);       
        i = +f2(5.5,1);
        return +i;
    }
    
    return f3;
}

var global = {}
var env = {fun2:function(x){print(x);}}

var asmModule = AsmModule(global,env)
print(asmModule  ( 1))

