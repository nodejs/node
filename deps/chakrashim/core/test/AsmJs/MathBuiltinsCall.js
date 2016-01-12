//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function foo() {}

var all = [ undefined, null,
            true, false, new Boolean(true), new Boolean(false),
            NaN, +0, -0, 0, 1, 10.0, 10.1, -1, -5, 5,
            124, 248, 654, 987, -1026, +98768.2546, -88754.15478,
            1<<32, -(1<<32), (1<<32)-1, 1<<31, -(1<<31), 1<<25, -1<<25, 65536, 46341,
            Number.MIN_VALUE, Number.NaN, Number.POSITIVE_INFINITY, Number.NEGATIVE_INFINITY,
            new Number(NaN), new Number(+0), new Number( -0), new Number(0), new Number(1),
            new Number(10.0), new Number(10.1), 
            new Number(Number.MIN_VALUE), new Number(Number.NaN), 
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

    var fround = stdlib.Math.fround ;


    var acos = stdlib.Math.acos ;
    var asin = stdlib.Math.asin ;
    var atan = stdlib.Math.atan ;
    var cos = stdlib.Math.cos  ;
    var sin = stdlib.Math.sin  ;
    var tan = stdlib.Math.tan  ;
    var ceil = stdlib.Math.ceil ;
    var floor = stdlib.Math.floor;
    var exp = stdlib.Math.exp  ;
    var log = stdlib.Math.log  ;
    var sqrt = stdlib.Math.sqrt ;
    // stdlib math (signed) -> signed ^ (doublish) -> double
    var abs = stdlib.Math.abs;
    // stdlib math (doublish, doublish) -> double
    var atan2 = stdlib.Math.atan2;
    var pow = stdlib.Math.pow;
    // stdlib math (int,int) -> signed
    var imul = stdlib.Math.imul;
    // stdlib math imm variable double
    var E = stdlib.Math.E;
    var LN10 = stdlib.Math.LN10;
    var LN2 = stdlib.Math.LN2;
    var LOG2E = stdlib.Math.LOG2E;
    var LOG10E = stdlib.Math.LOG10E;
    var PI = stdlib.Math.PI;
    var SQRT1_2 = stdlib.Math.SQRT1_2;
    var SQRT2 = stdlib.Math.SQRT2;
    
    //views
    var a=new stdlib.Int8Array(buffer);
    var b=new stdlib.Int16Array(buffer);
    var c=new stdlib.Int32Array(buffer);
    var d=new stdlib.Uint8Array(buffer);
    var e=new stdlib.Uint16Array(buffer);
    var f=new stdlib.Uint32Array(buffer);
    var g=new stdlib.Float32Array(buffer);
    var h=new stdlib.Float64Array(buffer);
    
    function f1(x){
        x = +x;
        return +acos(x);
    }
    
    function f2(x){
        x = +x;
        return +asin(x);
    }
    
    function f3(x){
        x = +x;
        return +atan(x);
    }
    
    function f4(x){
        x = +x;
        return +cos(x);
    }
    
    function f5(x){
        x = +x;
        return +sin(x);
    }
    
    function f6(x){
        x = +x;
        return +tan(x);
    }
    
    function f7(x){
        x = +x;
        return +ceil(x);
    }
    
    function f8(x){
        x = +x;
        return +floor(x);
    }
    
    function f9(x){
        x = +x;
        return +exp(x);
    }
    
    function f10(x){
        x = +x;
        return +log(x);
    }
    
    function f11(x){
        x = +x;
        return +sqrt(x);
    }
    
    // stdlib math (signed) -> signed ^ (doublish) -> double
    function f12(x){
        x = +x;
        return +abs(x);
    }
    function f13(x){
        x = x|0;
        return abs(x|0)|0;
    }
    
    // stdlib math (doublish, doublish) -> double
    function f14(x,y){
        x = +x;
        y = +y;
        return +atan2(x,y);
    }
    
    function f15(x,y){
        x = +x;
        y = +y;
        return +pow(x,y);
    }
    // stdlib math (int,int) -> signed
    function f16(x,y){
        x = x|0;
        y = y|0;
        return imul(x,y)|0;
    }
     
    function f17(x){
        x = fround(x);
        return fround(ceil(x));
    }
    
    function f18(x){
        x = fround(x);
        return fround(floor(x));
    }
    
    function f19(x){
        x = fround(x);
        return fround(sqrt(x));
    }
    
    function f20(x){
        x = fround(x);
        return fround(abs(x));
    }
    
    
    return { 
        f1  : f1  ,
        f2  : f2  ,
        f3  : f3  ,
        f4  : f4  ,
        f5  : f5  ,
        f6  : f6  ,
        f7  : f7  ,
        f8  : f8  ,
        f9  : f9  ,
        f10 : f10 ,
        f11 : f11 ,
        f12 : f12 ,
        f13 : f13 ,
        f14 : f14 ,
        f15 : f15 ,
        f16 : f16 ,
        f17 : f17 ,
        f18 : f18 ,
        f19 : f19 ,
        f20 : f20 ,
    };
}
var global = {Math:Math,Int8Array:Int8Array,Int16Array:Int16Array,Int32Array:Int32Array,Uint8Array:Uint8Array,Uint16Array:Uint16Array,Uint32Array:Uint32Array,Float32Array:Float32Array,Float64Array:Float64Array,Infinity:Infinity, NaN:NaN}
var env = {fun1:function(x){/*print(x);*/}, fun2:function(x,y){/*print(x,y);*/},i1:155,i2:658,d1:68.25,d2:3.14156,f1:48.1523,f2:14896.2514}
var buffer = new ArrayBuffer(1<<20);

var asmModule = AsmModule(global,env,buffer);

for (var i=0; i<all.length; ++i) {
    print("f1   (a["+i+"](" + all[i] +"))= " + (asmModule.f1   (all[i])));
    print("f2   (a["+i+"](" + all[i] +"))= " + (asmModule.f2   (all[i])));
    print("f3   (a["+i+"](" + all[i] +"))= " + (asmModule.f3   (all[i])));
    print("f4   (a["+i+"](" + all[i] +"))= " + Math.round(asmModule.f4   (all[i])));
    print("f5   (a["+i+"](" + all[i] +"))= " + Math.round(asmModule.f5   (all[i])));
    print("f6   (a["+i+"](" + all[i] +"))= " + Math.round(asmModule.f6   (all[i])));
    print("f7   (a["+i+"](" + all[i] +"))= " + (asmModule.f7   (all[i])));
    print("f8   (a["+i+"](" + all[i] +"))= " + (asmModule.f8   (all[i])));
    print("f9   (a["+i+"](" + all[i] +"))= " + (asmModule.f9   (all[i])));
    print("f10  (a["+i+"](" + all[i] +"))= " + (asmModule.f10  (all[i])));
    print("f11  (a["+i+"](" + all[i] +"))= " + (asmModule.f11  (all[i])));
    print("f12  (a["+i+"](" + all[i] +"))= " + (asmModule.f12  (all[i])));
    print("f13  (a["+i+"](" + all[i] +"))= " + (asmModule.f13  (all[i])));
    print("f17  (a["+i+"](" + all[i] +"))= " + (asmModule.f17  (all[i])));
    print("f18  (a["+i+"](" + all[i] +"))= " + (asmModule.f18  (all[i])));
    print("f19  (a["+i+"](" + all[i] +"))= " + (asmModule.f19  (all[i])));
    print("f20  (a["+i+"](" + all[i] +"))= " + (asmModule.f20  (all[i])));
}
for (var i=0; i<all.length; ++i) {
    for (var j=0; j<all.length; ++j) {
        // rounding atan2, because crt call gives us slightly different precision
        print("f14  (a["+i+"](" + all[i] +") , a["+j+"](" + all[j] +") )= " + Math.round(asmModule.f14  (all[i],all[j])));
        print("f15  (a["+i+"](" + all[i] +") , a["+j+"](" + all[j] +") )= " + Math.round(asmModule.f15  (all[i],all[j]))|0);
        print("f16  (a["+i+"](" + all[i] +") , a["+j+"](" + all[j] +") )= " + (asmModule.f16  (all[i],all[j])));
    }
}

