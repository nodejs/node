//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function foo() {}

var all = [ undefined, null,
            true, false, new Boolean(true), new Boolean(false),
            NaN, +0, -0, 0, 1, 10.0, 10.1, -1, -5, 5,
            124, 248, 654, 987, -1026, +98768.2546, -88754.15478,
            1<<32, -(1<<32), (1<<32)-1, 1<<31, -(1<<31), 1<<25, -1<<25, 
            Number.MAX_VALUE, Number.MIN_VALUE, Number.NaN, Number.POSITIVE_INFINITY, Number.NEGATIVE_INFINITY,
            new Number(NaN), new Number(+0), new Number( -0), new Number(0), new Number(1),
            new Number(10.0), new Number(10.1), 
            new Number(Number.MAX_VALUE), new Number(Number.MIN_VALUE), new Number(Number.NaN), 
            new Number(Number.POSITIVE_INFINITY), new Number(Number.NEGATIVE_INFINITY),
            "", "hello", "hel" + "lo", "+0", "-0", "0", "1", "10.0", "10.1",
            new String(""), new String("hello"), new String("he" + "llo"),
            new Object(), [1,2,3], new Object(), [1,2,3] , foo
          ];

function AsmModule(stdlib,foreign,buffer) {
    "use asm";
    
    //views
    var HEAP8  =new stdlib.Int8Array(buffer);
    var HEAP16 =new stdlib.Int16Array(buffer);
    var HEAP32 =new stdlib.Int32Array(buffer);
    var HEAPU8 =new stdlib.Uint8Array(buffer);
    var HEAPU16=new stdlib.Uint16Array(buffer);
    var HEAPU32=new stdlib.Uint32Array(buffer);
    var HEAPF32=new stdlib.Float32Array(buffer);
    var HEAP64 =new stdlib.Float64Array(buffer);
        
    function read8  (x){x = x|0; return HEAP8  [0xFFF]   |0; }
    function read16 (x){x = x|0; return HEAP16 [0xFFF]|0; }
    function read32 (x){x = x|0; return HEAP32 [0xFFF]|0; }
    function readU8 (x){x = x|0; return +((HEAPU8[0xFFF])   >>>0); }
    function readU16(x){x = x|0; return +((HEAPU16[0xFFF])>>>0); }
    function readU32(x){x = x|0; return +((HEAPU32[0xFFF])>>>0); }
    function readF32(x){x = x|0; return +HEAPF32[0xFFF]; }
    function read64 (x){x = x|0; return +HEAP64 [0xFFF]; }
    return {
         read8   : read8  
        ,read16  : read16 
        ,read32  : read32 
        ,readU8  : readU8 
        ,readU16 : readU16
        ,readU32 : readU32
        ,readF32 : readF32
        ,read64  : read64 
    };
}

var stdlib = {Math:Math,Int8Array:Int8Array,Int16Array:Int16Array,Int32Array:Int32Array,Uint8Array:Uint8Array,Uint16Array:Uint16Array,Uint32Array:Uint32Array,Float32Array:Float32Array,Float64Array:Float64Array,Infinity:Infinity, NaN:NaN}
var env = {fun1:function(x){print(x);}, fun2:function(x,y){print(x,y);},i1:155,i2:658,d1:68.25,d2:3.14156,f1:48.1523,f2:14896.2514}
var buffer = new ArrayBuffer(1<<20);
var HEAP8  =new stdlib.Int8Array(buffer);
var HEAP16 =new stdlib.Int16Array(buffer);
var HEAP32 =new stdlib.Int32Array(buffer);
var HEAPU8 =new stdlib.Uint8Array(buffer);
var HEAPU16=new stdlib.Uint16Array(buffer);
var HEAPU32=new stdlib.Uint32Array(buffer);
var HEAPF32=new stdlib.Float32Array(buffer);
var HEAP64 =new stdlib.Float64Array(buffer);

var asmModule = AsmModule(stdlib,env,buffer);

var x = 0xFFF;
for (var j=0; j<all.length; ++j) {
    var y = all[j];
    HEAP8  [x]   = y|0;
    print("read8    HEAP8  ["+x+"]   = all["+j+"]("+y+"); = " + (asmModule.read8   (x)));
    HEAP16 [x]= y|0;
    print("read16   HEAP16 ["+x+"]   = all["+j+"]("+y+"); = " + (asmModule.read16  (x)));
    HEAP32 [x]= y|0;
    print("read32   HEAP32 ["+x+"]   = all["+j+"]("+y+"); = " + (asmModule.read32  (x)));
    HEAPU8 [x]   = y|0;
    print("readU8   HEAPU8 ["+x+"]   = all["+j+"]("+y+"); = " + (asmModule.readU8  (x)));
    HEAPU16[x]= y|0;
    print("readU16  HEAPU16["+x+"]   = all["+j+"]("+y+"); = " + (asmModule.readU16 (x)));
    HEAPU32[x]= y|0;
    print("readU32  HEAPU32["+x+"]   = all["+j+"]("+y+"); = " + (asmModule.readU32 (x)));
    HEAPF32[x]= +y;
    print("readF32  HEAPF32["+x+"]   = all["+j+"]("+y+"); = " + (asmModule.readF32 (x)));
    HEAP64 [x]= +y;
    print("read64   HEAP64 ["+x+"]   = all["+j+"]("+y+"); = " + (asmModule.read64  (x)));
}

