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

    // numerical mutable variable
    var i1 = 0, d1 = 0.0, i2 = -5;
    // foreign imports
    var fi1 = foreign.i1|0;
    var fi2 = foreign.i2|0;
    var fd1 = +foreign.d1;
    var fd2 = +foreign.d2;
    var fun1 = foreign.fun1;
    var fun2 = foreign.fun2;

    // stdlib immutable variable type double
    var sInf = stdlib.Infinity, sNaN = stdlib.NaN;
    // stdlib math (double) -> double
    var m1 = stdlib.Math.acos;
    var fround = stdlib.Math.fround;
    var m2 = stdlib.Math.asin;
    var m3 = stdlib.Math.atan;
    var m4 = stdlib.Math.cos;
    var m5 = stdlib.Math.sin;
    var m6 = stdlib.Math.tan;
    var m7 = stdlib.Math.ceil;
    var m8 = stdlib.Math.floor;
    var m9 = stdlib.Math.exp;
    var m10 = stdlib.Math.log;
    var m11 = stdlib.Math.sqrt;
    // stdlib math (signed) -> signed ^ (doublish) -> double
    var m12 = stdlib.Math.abs;
    // stdlib math (doublish, doublish) -> double
    var m13 = stdlib.Math.atan2;
    var m34 = stdlib.Math.pow;
    // stdlib math (int,int) -> signed
    var m14 = stdlib.Math.imul;
    // stdlib math imm variable double
    var m15 = stdlib.Math.E;
    var m16 = stdlib.Math.LN10;
    var m17 = stdlib.Math.LN2;
    var m18 = stdlib.Math.LOG2E;
    var m19 = stdlib.Math.LOG10E;
    var m20 = stdlib.Math.PI;
    var m21 = stdlib.Math.SQRT1_2;
    var m22 = stdlib.Math.SQRT2;

    //views
    var HEAP8  =new stdlib.Int8Array(buffer);
    var HEAP16 =new stdlib.Int16Array(buffer);
    var HEAP32 =new stdlib.Int32Array(buffer);
    var HEAPU8 =new stdlib.Uint8Array(buffer);
    var HEAPU16=new stdlib.Uint16Array(buffer);
    var HEAPU32=new stdlib.Uint32Array(buffer);
    var HEAPF32=new stdlib.Float32Array(buffer);
    var HEAP64 =new stdlib.Float64Array(buffer);

    function write8  (x,y){x = x|0; y = y|0; HEAP8  [x]    = y; }
    function write16 (x,y){x = x|0; y = y|0; HEAP16 [x>>1] = y; }
    function write32 (x,y){x = x|0; y = y|0; HEAP32 [x>>2] = y; }
    function writeU8 (x,y){x = x|0; y = y|0; HEAPU8 [x]    = y; }
    function writeU16(x,y){x = x|0; y = y|0; HEAPU16[x>>1] = y; }
    function writeU32(x,y){x = x|0; y = y|0; HEAPU32[x>>2] = y; }
    function writeF32(x,y){x = x|0; y = +y;  HEAPF32[x>>2] = y; }
    function writeF32f(x,y){x = x|0; y = fround(y);  HEAPF32[x>>2] = y; }
    function write64 (x,y){x = x|0; y = +y;  HEAP64 [x>>3] = y; }

    return {
         write8   : write8
        ,write16  : write16
        ,write32  : write32
        ,writeU8  : writeU8
        ,writeU16 : writeU16
        ,writeU32 : writeU32
        ,writeF32 : writeF32
        ,writeF32f: writeF32f
        ,write64  : write64
    };
}

var stdlib = {Math:Math,Int8Array:Int8Array,Int16Array:Int16Array,Int32Array:Int32Array,Uint8Array:Uint8Array,Uint16Array:Uint16Array,Uint32Array:Uint32Array,Float32Array:Float32Array,Float64Array:Float64Array,Infinity:Infinity, NaN:NaN}
var env = {fun1:function(x){print(x);}, fun2:function(x,y){print(x,y);},i1:155,i2:658,d1:68.25,d2:3.14156,f1:48.1523,f2:14896.2514}
var buffer =new ArrayBuffer(1<<26); // smallest allocation that allows the test to succeed
var HEAP8  =new stdlib.Int8Array(buffer);
var HEAP16 =new stdlib.Int16Array(buffer);
var HEAP32 =new stdlib.Int32Array(buffer);
var HEAPU8 =new stdlib.Uint8Array(buffer);
var HEAPU16=new stdlib.Uint16Array(buffer);
var HEAPU32=new stdlib.Uint32Array(buffer);
var HEAPF32=new stdlib.Float32Array(buffer);
var HEAP64 =new stdlib.Float64Array(buffer);

var asmModule = AsmModule(stdlib,env,buffer);

for (var i=0; i<all.length; ++i) {
    var x = all[i]|0;
    for (var j=0; j<all.length; ++j) {
        var y = all[j];
        asmModule.write8   (x, y); print("write8    HEAP8  ["+x+"]   = all["+j+"]("+y+"); = " + (HEAP8  [x]   |0));
        asmModule.writeF32f(x, y); print("writeF32f HEAPF32["+x+"]   = all["+j+"]("+y+"); = " + (Math.fround(HEAPF32[x>>2]) ));
        asmModule.write16  (x, y); print("write16   HEAP16 ["+x+"]   = all["+j+"]("+y+"); = " + (HEAP16 [x>>1]|0));
        asmModule.write32  (x, y); print("write32   HEAP32 ["+x+"]   = all["+j+"]("+y+"); = " + (HEAP32 [x>>2]|0));
        asmModule.writeU8  (x, y); print("writeU8   HEAPU8 ["+x+"]   = all["+j+"]("+y+"); = " + (HEAPU8 [x]   |0));
        asmModule.writeF32 (x, y); print("writeF32  HEAPF32["+x+"]   = all["+j+"]("+y+"); = " + (+HEAPF32[x>>2] ));
        asmModule.writeU16 (x, y); print("writeU16  HEAPU16["+x+"]   = all["+j+"]("+y+"); = " + (HEAPU16[x>>1]|0));
        asmModule.writeU32 (x, y); print("writeU32  HEAPU32["+x+"]   = all["+j+"]("+y+"); = " + (HEAPU32[x>>2]|0));
        asmModule.write64  (x, y); print("write64   HEAP64 ["+x+"]   = all["+j+"]("+y+"); = " + (+HEAP64 [x>>3] ));
    }
}
