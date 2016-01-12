//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function AsmModule(stdlib,foreign,buffer) {
    "use asm";
    var fround = stdlib.Math.fround;
    var a = 2.0;
    const b = 1;
    var c = fround(1.0);
    const d = fround(1);
    
    function f1(){
        var e = d;
        var f = fround(4);
        e = fround(e+c);
        return fround(e);
    }
    
    function f2(){
        return d;
    }
    
    return { 
        f1 : f1,
        f2 : f2
    };
}

var stdlib = {Math:Math,Int8Array:Int8Array,Int16Array:Int16Array,Int32Array:Int32Array,Uint8Array:Uint8Array,Uint16Array:Uint16Array,Uint32Array:Uint32Array,Float32Array:Float32Array,Float64Array:Float64Array,Infinity:Infinity, NaN:NaN}
var env = {}
var buffer = new ArrayBuffer(1<<20);

var asmModule = AsmModule(stdlib,env,buffer);
print(asmModule.f1());
print(asmModule.f2());
