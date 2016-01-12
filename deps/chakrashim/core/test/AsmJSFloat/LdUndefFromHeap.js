//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

//2nd test case
function AsmModule(stdlib,foreign,buffer) {
    "use asm";
    var m1 = stdlib.Math.fround;
    var func1 = foreign.fun1;
    //views
    var HEAP8  =new stdlib.Float32Array(buffer);

    function f1(){
        var x = m1(1.5);
        x = m1(HEAP8[1]);
        return m1(x)

    }
    return f1;
}

var global = {Math:Math,Float32Array:Float32Array}
var env = {fun1:function(x1,x2,x3,x4,x5,x6,x7,x8){print(x1,x2,x3,x4,x5,x6,x7,x8);}, fun2:function(x,y){print(x,y);},x:155,i2:658,d1:68.25,d2:3.14156,f1:48.1523,f2:14896.2514}
var buffer = new ArrayBuffer(1<<20);

var asmModule = AsmModule(global,env,buffer);

WScript.Echo(asmModule());
WScript.Echo(asmModule());
