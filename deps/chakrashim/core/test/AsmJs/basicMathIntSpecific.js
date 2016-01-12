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

    function Or(x,y) {
        x = x|0;
        y = y|0;
        return (x|y)|0;
    }
    function And(x,y) {
        x = x|0;
        y = y|0;
        return (x&y)|0;
    }
    function Rsh(x,y) {
        x = x|0;
        y = y|0;
        return (x>>y)|0;
    }
    function Lsh(x,y) {
        x = x|0;
        y = y|0;
        return (x<<y)|0;
    }
    function Xor(x,y) {
        x = x|0;
        y = y|0;
        return (x^y)|0;
    }
    function RshU(x,y) {
        x = x|0;
        y = y|0;
        return (x>>>y)|0;
    }
    function Rem(x,y) {
        x = x|0;
        y = y|0;
        return ((x|0)%(y|0))|0;
    }

    return {
        Or  :Or
        ,And :And
        ,Rsh :Rsh
        ,Lsh :Lsh
        ,Xor :Xor
        ,RshU:RshU
        ,Rem:Rem
    };
}

var asmModule = AsmModule();     // produces AOT-compiled version
for (var i=0; i<all.length; ++i) {
    for (var j=0; j<all.length; ++j) {
        print("a["+i+"](" + all[i] +") |   a["+j+"]("+all[j]+") = " + (asmModule.Or(all[i],all[j])));
        print("a["+i+"](" + all[i] +") &   a["+j+"]("+all[j]+") = " + (asmModule.And(all[i],all[j])));
        print("a["+i+"](" + all[i] +") >>  a["+j+"]("+all[j]+") = " + (asmModule.Rsh(all[i],all[j])));
        print("a["+i+"](" + all[i] +") <<  a["+j+"]("+all[j]+") = " + (asmModule.Lsh(all[i],all[j])));
        print("a["+i+"](" + all[i] +") ^   a["+j+"]("+all[j]+") = " + (asmModule.Xor(all[i],all[j])));
        print("a["+i+"](" + all[i] +") >>> a["+j+"]("+all[j]+") = " + (asmModule.RshU(all[i],all[j])));
        print("a["+i+"](" + all[i] +") %   a["+j+"]("+all[j]+") = " + (asmModule.Rem(all[i],all[j])));
    }
}
