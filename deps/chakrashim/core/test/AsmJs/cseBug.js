//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function AsmModule(stdlib,foreign,buffer) {
    "use asm";
    //views
    var HEAP8  =new stdlib.Int8Array(buffer);
    var HEAP32 =new stdlib.Int32Array(buffer);
        
    function read8  (x){
        x = x|0; 
        var y = 0;
        var z = 0;
        HEAP32[x>>2] = 257;
        y = HEAP8[x]|0;
        z = HEAP32[x >> 2]|0;
        return (y + z)|0; 
    }
    return read8
}
var stdlib = {Math:Math,Int8Array:Int8Array,Int16Array:Int16Array,Int32Array:Int32Array,Uint8Array:Uint8Array,Uint16Array:Uint16Array,Uint32Array:Uint32Array,Float32Array:Float32Array,Float64Array:Float64Array,Infinity:Infinity, NaN:NaN}
var env = {}
var buffer = new ArrayBuffer(1<<20);
var asmModule = AsmModule(stdlib,env,buffer);
print(asmModule(0));
print(asmModule(0));
