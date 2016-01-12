//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function AsmModule(glob) {
    "use asm";
    var fround = glob.Math.fround;
    function floatNeg(a) {
        a = fround(a);
        var e = fround(0);
        e = fround(-a);
        return fround(e);
    }
    function uintRem(a) {
        a = a|0;
        var b = 0;
        var c = 0;
        var d = 0;
        c = (a + d)|0;
        d = (a >>> 0) % 10 | 0;
        b = c;
        return b|0;
    }
    return {
        floatNeg:floatNeg,
        uintRem:uintRem
    }
}
var global = {Math:Math,Int8Array:Int8Array,Int16Array:Int16Array,Int32Array:Int32Array,Uint8Array:Uint8Array,Uint16Array:Uint16Array,Uint32Array:Uint32Array,Float32Array:Float32Array,Float64Array:Float64Array,Infinity:Infinity, NaN:NaN}
var module = AsmModule(global);
print(module.floatNeg(3));
print(module.uintRem(400));