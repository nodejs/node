//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function AsmModule(stdlib,foreign,buffer) {
    "use asm";
    var fround = stdlib.Math.fround;
    var f32 = new stdlib.Float32Array(buffer);
    var f64 = new stdlib.Float64Array(buffer);
    
    var h = 0.0;
    var f = fround(-0);
    var g = -0.;
    function f1(){
        var a = -0.0;
        var b = 0.0;
        var c = -0.0;
        var d = 0.0;
        c = 0.0;
        d = -0.0;
        f64[0] = a;
        f64[1] = b;
        f64[2] = c;
        f64[3] = d;
        f64[4] = g;
        return +f64[0];
    }
    function f2(){
        var a = fround(-0);
        var b = fround(-0.);
        var c = fround(0);
        var d = fround(0);
        var e = fround(0);
        c = fround(-0);
        d = fround(-0.);
        e = fround(0);
        f32[9] = fround(a);
        f32[10] = fround(b);
        f32[11] = fround(c);
        f32[12] = fround(d);
        f32[13] = fround(e);
        f32[14] = fround(f);
        return fround(f32[9]);
    }
    
    return { 
        f1 : f1,
        f2 : f2
    };
}
var stdlib = this;
var env = {}
var buffer = new ArrayBuffer(1<<20);
var asmModule = AsmModule(stdlib,env,buffer);
print(asmModule.f1());
print(asmModule.f2());
var int8Arr = new stdlib.Int8Array(buffer);
// check that sign is set
print(int8Arr[7]);
print(int8Arr[15]);
print(int8Arr[23]);
print(int8Arr[31]);
print(int8Arr[39]);
print(int8Arr[43]);
print(int8Arr[47]);
print(int8Arr[51]);
print(int8Arr[55]);
print(int8Arr[59]);