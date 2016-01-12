//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function AsmModule(stdlib,foreign,buffer) {
    "use asm";
    function add(x,y) {
        x = +x;
        y = +y;
        return +(x+y);
    }
    
    function f2(x,y){
        x = +x;
        y = +y;
        var i = 0.0;
        var t = 1;
        i = +fTableDbOp[t&3](x,y);
        return +i;
    }
    
    var fTableDbOp = [add,add,add,add];
    
    
    return { 
        f2 : f2
    };
}

var stdlib = {Math:Math,Int8Array:Int8Array,Int16Array:Int16Array,Int32Array:Int32Array,Uint8Array:Uint8Array,Uint16Array:Uint16Array,Uint32Array:Uint32Array,Float32Array:Float32Array,Float64Array:Float64Array,Infinity:Infinity, NaN:NaN}
var env = {}
var buffer = new ArrayBuffer(1<<20);

var asmModule = AsmModule(stdlib,env,buffer);
var x = 1;
var y = 2;
WScript.Echo(asmModule.f2(x,y));
WScript.Echo(asmModule.f2(x,y));