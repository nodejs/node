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

function AsmModule() {
    "use asm";

    function sum(x,y) {
        x = x|0;
        y = +y;
        if( (x|0) > 10){
            x = 10;
        }
        while( (x|0) > 0){
            x = (x - 1)|0;
            y = +(y + 1.0);
            if( (x|0) == 0 ){
                break;
            }
        }
        return +y;
    }

    function sumWhile(x,y) {
        x = x|0;
        y = +y;
        if( (x|0) > 10){
            x = 10;
        }
        do{
            x = (x - 1)|0;
            if((x|0) > 10){
                x = 10;
            }
            y = +(y + 1.0);
            if( (x|0) == 0 ){
                break;
            }
        }
        while( (x|0) > 0 );

        return +y;
    }

    function forLoop(x,y) {
        x = x|0;
        y = y|0;
        var i=0;

        if( (x|0) > 10){
            x = 10;
        }
        if( (y|0) <= 0){
            y = 1;
        }
        for(; (i|0) < (x|0) ; i = (i+1)|0) {
            y = (y + 1)|0;
        }

        for(i=0; (i|0) < (x|0) ; i = (i+1)|0 ) {
            y = (y + 2)|0;
        }

        for(;;){
            y = y << 1;
            if( (y|0) > (1<<20)) {
                break;
            }
        }
        return y|0;
    }

    return {
        f1 : sum,
        f2 : sumWhile,
        f3 : forLoop,
    };
}


var asmModule = AsmModule();


for (var i=0; i<all.length; ++i) {
    for (var j=0; j<all.length; ++j) {
        print("f1 a["+i+"](" + all[i] +") , a["+j+"]("+all[j]+") = " + (asmModule.f1(all[i],all[j])));
        print("f2 a["+i+"](" + all[i] +") , a["+j+"]("+all[j]+") = " + (asmModule.f2(all[i],all[j])));
        print("f3 a["+i+"](" + all[i] +") , a["+j+"]("+all[j]+") = " + (asmModule.f3(all[i],all[j])));
    }
}
