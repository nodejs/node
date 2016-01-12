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

    function posInt(x) {
        x = x|0;
        return +(+(x|0));
    }
    function posUInt(x) {
        x = x|0;
        return +(+(x>>>0));
    }
    function posDouble(x) {
        x = +x;
        return +(+x);
    }

    function posFloat(x) {
        x = fround(x);
        return fround(+x);
    }

    function negInt(x) {
        x = x|0;
        return (-x)|0;
    }
    function negDouble(x) {
        x = +x;
        return +(-x);
    }
    function negFloat(x) {
        x = fround(x);
        return fround(-x);
    }

    function bitnotInt(x) {
        x = x|0;
        return (~x)|0;
    }

    function bitnotnotInt(x) {
        x = +x;
        return (~~x)|0;
    }

    function lognotInt(x) {
        x = x|0;
        return (!x)|0;
    }

    function lognot2Int(x) {
        x = x|0;
        return (!!x)|0;
    }

    return {
        posInt       : posInt       ,
        posUInt      : posUInt      ,
        posDouble    : posDouble    ,
        posFloat     : posFloat    ,
        negInt       : negInt       ,
        negDouble    : negDouble    ,
        negFloat     : negFloat    ,
        bitnotInt    : bitnotInt    ,
        bitnotnotInt : bitnotnotInt ,
        lognotInt    : lognotInt    ,
        lognot2Int   : lognot2Int    ,
    };
}


var asmModule = AsmModule({Math:Math});

for (var i=0; i<all.length; ++i) {
    print("i   +a["+i+"](" + all[i] +") = " + (asmModule.posInt       (all[i])));
    print("ui  +a["+i+"](" + all[i] +") = " + (asmModule.posUInt      (all[i])));
    print("d   +a["+i+"](" + all[i] +") = " + (asmModule.posDouble    (all[i])));
    print("f   +a["+i+"](" + all[i] +") = " + (asmModule.posFloat     (all[i])));
    print("i   -a["+i+"](" + all[i] +") = " + (asmModule.negInt       (all[i])));
    print("d   -a["+i+"](" + all[i] +") = " + (asmModule.negDouble    (all[i])));
    print("f   -a["+i+"](" + all[i] +") = " + (asmModule.negFloat     (all[i])));
    print("i   ~a["+i+"](" + all[i] +") = " + (asmModule.bitnotInt    (all[i])));
    print("i  ~~a["+i+"](" + all[i] +") = " + (asmModule.bitnotnotInt (all[i])));
    print("i   !a["+i+"](" + all[i] +") = " + (asmModule.lognotInt    (all[i])));
    print("i  !!a["+i+"](" + all[i] +") = " + (asmModule.lognot2Int   (all[i])));
}
