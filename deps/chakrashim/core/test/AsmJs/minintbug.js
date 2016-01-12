//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function AsmModule(stdlib,foreign,buffer) {
    "use asm";
    var bb = foreign.a;
    var abs = stdlib.Math.abs;
    function f1(){
        var b = 1.0;
        if(b>2147483647.0|!(b>=-2147483648.0)){
            b = 5.0;
        }
        return +b;
    }
    function f2(){
        var a = 0;
        a = abs(-2147483648)|0;
        return a|0;
    }
    
    return { 
        f1 : f1,
        f2 : f2
    };
}

var stdlib = {Math:Math,Int8Array:Int8Array,Int16Array:Int16Array,Int32Array:Int32Array,Uint8Array:Uint8Array,Uint16Array:Uint16Array,Uint32Array:Uint32Array,Float32Array:Float32Array,Float64Array:Float64Array,Infinity:Infinity, NaN:NaN}
var env = {"a":function(x){print(x);}}
var buffer = new ArrayBuffer(1<<20);

var asmModule = AsmModule(stdlib,env,buffer);
print(asmModule.f1());
print(asmModule.f2());
