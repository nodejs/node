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

function AsmModule(stdlib) {
    "use asm";

    var fround = stdlib.Math.fround;

    function brInt(x,y) {
        x = x|0;
        y = y|0;
        var i = 0;
        if( (y|0) < 0 & (x|0) < 0){
            i = 1;
        }
        else {
            if((x|0)>0 | (y|0)>0){
                i = 2;
            }
            else{
                i = 3;
            }
        }
        return (i)|0;
    }

    function brDouble(x,y) {
        x = +x;
        y = +y;
        var i = 0;
        if(y<0.0 & x<0.0){
            i = 1;
        }
        else {
            if(x>0.0 | y>0.0){
                i = 2;
            }
            else{
                i = 3;
            }
        }
        return (i)|0;
    }

    function brFloat(x,y) {
        x = fround(x);
        y = fround(y);
        var i = 0;
        if(y<fround(0.0) & x<fround(0.0)){
            i = 1;
        }
        else {
            if(x>fround(0.0) | y>fround(0.0)){
                i = 2;
            }
            else{
                i = 3;
            }
        }
        return (i)|0;
    }

    function brMix(x,y) {
        x = x|0;
        y = +y;
        var i = 0;
        if((x|0)>0){
            if(y>0.0){
                i = 1;
            } else {
                if(y==0.0){
                    i = 2;
                } else {
                    i = 3;
                }
            }
            return i|0;
        }
        else {
            if((x|0)==0){
                if(y>0.0){
                    i = 4;
                } else {
                    if(y==0.0){
                        i = 5;
                    } else {
                        i = 6;
                    }
                }
            } else { // x < 0
                if(y>0.0){
                    i = 7;
                } else {
                    if(y==0.0){
                        i = 8;
                    } else {
                        i = 9;
                    }
                }
            }
            return i|0;
        }
        return (i)|0;
    }

    return {
        brInt   : brInt,
        brDouble: brDouble,
        brFloat : brFloat,
        brMix   : brMix
    };
}


var asmModule = AsmModule({Math:Math});

for (var i=0; i<all.length; ++i) {
    print("t1   +a["+i+"](" + all[i] +") = " + (asmModule.brInt   (all[i])));
    print("t2   +a["+i+"](" + all[i] +") = " + (asmModule.brDouble(all[i])));
    print("t2   +a["+i+"](" + all[i] +") = " + (asmModule.brFloat(all[i])));
    print("t3   +a["+i+"](" + all[i] +") = " + (asmModule.brMix   (all[i])));
}

for (var i=0; i<all.length; ++i) {
    for (var j=0; j<all.length; ++j) {
        print("t1 a["+i+"](" + all[i] +") , a["+j+"]("+all[j]+") = " + (asmModule.brInt   (all[i],all[j])));
        print("t2 a["+i+"](" + all[i] +") , a["+j+"]("+all[j]+") = " + (asmModule.brDouble(all[i],all[j])));
        print("t2 a["+i+"](" + all[i] +") , a["+j+"]("+all[j]+") = " + (asmModule.brFloat (all[i],all[j])));
        print("t3 a["+i+"](" + all[i] +") , a["+j+"]("+all[j]+") = " + (asmModule.brMix   (all[i],all[j])));
    }
}
