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

    function f1(x,y) {
        x = x|0;
        y = y|0;
        var i = 0;

        switch(x|0){
        case 0:
            i = 0;
        break;

        case 1:
        case 2:
            i = 1;
        break;

        case 5:
            i = 5;
        break;

        case 3:
            i = 3;
        break;

        default:
            i = 7;
        break;
        }

        return i|0;
    }

    function f2(x,y) {
        x = x|0;
        y = y|0;
        var i = 0;

        switch(x|0){
        case 0:
            i = 0;
        break;

        case 1:
        case 2:
            i = 1;
        break;

        case 5:
            i = 5;
        break;

        case 3:
            i = 3;
        break;
        case -2147483648:
            i = 3;
        break;

        case 2147483647:
            i = 3;
        break;

        }

        return i|0;
    }

    return {
        f1 : f1,
        f2 : f2,
    };
}


var asmModule = AsmModule();

for (var i=0; i<all.length; ++i) {
    print("f1 a["+i+"](" + all[i] +") = " + (asmModule.f1   (all[i])));
    print("f2 a["+i+"](" + all[i] +") = " + (asmModule.f2   (all[i])));
}

