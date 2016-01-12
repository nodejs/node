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
    var m1 = stdlib.Math.acos ;
    var m2 = stdlib.Math.asin ;
    var m3 = stdlib.Math.atan ;
    var m4 = stdlib.Math.cos  ;
    var m5 = stdlib.Math.sin  ;
    var m6 = stdlib.Math.tan  ;
    var m7 = stdlib.Math.ceil ;
    var m8 = stdlib.Math.floor;
    var m9 = stdlib.Math.exp  ;
    var m10 = stdlib.Math.log  ;
    var m11 = stdlib.Math.sqrt ;
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
    var a=new stdlib.Int8Array(buffer);
    var b=new stdlib.Int16Array(buffer);
    var c=new stdlib.Int32Array(buffer);
    var d=new stdlib.Uint8Array(buffer);
    var e=new stdlib.Uint16Array(buffer);
    var f=new stdlib.Uint32Array(buffer);
    var g=new stdlib.Float32Array(buffer);
    var h=new stdlib.Float64Array(buffer);
    
    function LtInt(x,y) {
        x = x|0;
        y = y|0;
        return ((x|0)<(y|0))|0;
    }
    
    function LeInt(x,y) {
        x = x|0;
        y = y|0;
        return ((x|0)<=(y|0))|0;
    }
   
    function GtInt(x,y) {
        x = x|0;
        y = y|0;
        return ((x|0)>(y|0))|0;
    }
    
    function GeInt(x,y) {
        x = x|0;
        y = y|0;
        return ((x|0)>=(y|0))|0;
    }
    
    function EqInt(x,y) {
        x = x|0;
        y = y|0;
        return ((x|0)==(y|0))|0;
    }
   
    function NeInt(x,y) {
        x = x|0;
        y = y|0;
        return ((x|0)!=(y|0))|0;
    }
    
    function LtDouble(x,y) {
        x = +x;
        y = +y;
        return (x<y)|0;
    }
    
   
    function LeDouble(x,y) {
        x = +x;
        y = +y;
        return (x<=y)|0;
    }
   
    function GtDouble(x,y) {
        x = +x;
        y = +y;
        return (x>y)|0;
    }
  
    function GeDouble(x,y) {
        x = +x;
        y = +y;
        return (x>=y)|0;
    }
    
  
    function EqDouble(x,y) {
        x = +x;
        y = +y;
        return (x==y)|0;
    }
    
  
    function NeDouble(x,y) {
        x = +x;
        y = +y;
        return (x!=y)|0;
    }
    
    function iadd(x,y) {
        x = x|0;
        y = y|0;
        return (x+y)|0;
    }
    function isub(x,y) {
        x = x|0;
        y = y|0;
        return (x-y)|0;
    }
   
    function imul(x,y) {
        x = x|0;
        y = y|0;
        var i = 0;
        var r = 0;
        
        if( y|0 > 10 ){
            y = 10;
        }   
        for(;(i|0)<(y|0);i = (i+1)|0){
            r = (r + x)|0;
        }
        return r|0;
    }
  
    function idiv(x,y) {
        x = x|0;
        y = y|0;
        return ((x>>>0)/(y>>>0))|0;
    }
    
    function add(x,y) {
        x = +x;
        y = +y;
        return +(x+y);
    }

    function sub(x,y) {
        x = +x;
        y = +y;
        return +(x-y);
    }

    function mul(x,y) {
        x = +x;
        y = +y;
        return +(x*y);
    }

    function div(x,y) {
        x = +x;
        y = +y;
        return +(x/y);
    }
    
    function f1(x,y){
        x = x|0;
        y = y|0;
        var i = 0, j = 0;
        if(EqInt(x,y)|0){
            i = imul(x,iadd(x,x)|0)|0;
            j = imul(idiv(x,5)|0,imul(x,2)|0)|0;
        }
        else {
            i = imul(x,iadd(x,y)|0)|0;
            j = imul(idiv(x,5)|0,iadd(x,y)|0)|0;
        }
        if(LtInt(i,j)|0){
            i = j;
        }
        else{
            fun1(j|0);
        }
        return i|0;
    }
    
    function f2(x,y){
        x = +x;
        y = +y;
        var i = 0.0, j = 0.0;
        if(EqDouble(x,y)|0){
            i = +mul(x,+add(x,x));
            j = +mul(+div(x,5.5),+mul(x,2.1));
        }
        else {
            i = +mul(x,+add(x,y));
            j = +mul(+div(x,5.5),+add(x,y));
        }
        if(LtDouble(i,j)|0){
            i = j;
        }
        else{
            fun1(j);
        }
        return +i;
    }
    
    function f3(x,y){
        x = x|0;
        y = +y;
        var i = 0, j = 0.0;
        i = f1(x,~~(+f2(y,5.5)))|0;
        j = +f2(y,+(f1(x,5)|0));
        fun2(i|0,j);
        return i|0;
    }
    
    var fTable = [f1,f1];
    var fTable2 = [f2,f2];
    
    
    return { 
        f1 : f1,
        f2 : f2,
        f3 : f3,
    };
}


var global = {Math:Math,Int8Array:Int8Array,Int16Array:Int16Array,Int32Array:Int32Array,Uint8Array:Uint8Array,Uint16Array:Uint16Array,Uint32Array:Uint32Array,Float32Array:Float32Array,Float64Array:Float64Array,Infinity:Infinity, NaN:NaN}
var env = {fun1:function(x){print(x);}, fun2:function(x,y){print(x,y);},i1:155,i2:658,d1:68.25,d2:3.14156,f1:48.1523,f2:14896.2514}
var buffer = new ArrayBuffer(1<<20);

var asmModule = AsmModule(global,env,buffer);

for (var i=0; i<all.length; ++i) {
    for (var j=0; j<all.length; ++j) {
        print("f1 (a["+i+"](" + all[i] +") , a["+j+"](" + all[j] +") )= " + (asmModule.f1   (all[i],all[j])));
        print("f2 (a["+i+"](" + all[i] +") , a["+j+"](" + all[j] +") )= " + (asmModule.f2   (all[i],all[j])));
        print("f3 (a["+i+"](" + all[i] +") , a["+j+"](" + all[j] +") )= " + (asmModule.f3   (all[i],all[j])));
    }
}

