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

function AsmModuleDouble() {
    "use asm";


    function LtFloat(x,y) {
        x = +x;
        y = +y;
        return (x<y)|0;
    }


    function LeFloat(x,y) {
        x = +x;
        y = +y;
        return (x<=y)|0;
    }

    function GtFloat(x,y) {
        x = +x;
        y = +y;
        return (x>y)|0;
    }

    function GeFloat(x,y) {
        x = +x;
        y = +y;
        return (x>=y)|0;
    }


    function EqFloat(x,y) {
        x = +x;
        y = +y;
        return (x==y)|0;
    }


    function NeFloat(x,y) {
        x = +x;
        y = +y;
        return (x!=y)|0;
    }

    return {
         Lt : LtFloat
        ,Le : LeFloat
        ,Gt : GtFloat
        ,Ge : GeFloat
        ,Eq : EqFloat
        ,Ne : NeFloat
    };
}

var asmModuleDouble = AsmModuleDouble();     // produces AOT-compiled version

print("Comparison for doubles");
for (var i=0; i<all.length; ++i) {
    for (var j=0; j<all.length; ++j) {
        print("d  a["+i+"](" + all[i] +") <  a["+j+"]("+all[j]+") = " + (asmModuleDouble.Lt(all[i],all[j])));
        print("d  a["+i+"](" + all[i] +") <= a["+j+"]("+all[j]+") = " + (asmModuleDouble.Le(all[i],all[j])));
        print("d  a["+i+"](" + all[i] +") >  a["+j+"]("+all[j]+") = " + (asmModuleDouble.Gt(all[i],all[j])));
        print("d  a["+i+"](" + all[i] +") >= a["+j+"]("+all[j]+") = " + (asmModuleDouble.Ge(all[i],all[j])));
        print("d  a["+i+"](" + all[i] +") == a["+j+"]("+all[j]+") = " + (asmModuleDouble.Eq(all[i],all[j])));
        print("d  a["+i+"](" + all[i] +") != a["+j+"]("+all[j]+") = " + (asmModuleDouble.Ne(all[i],all[j])));
    }
}

