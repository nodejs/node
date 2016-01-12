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
            Number.MAX_VALUE, Number.MIN_VALUE, Number.NaN, Number.POSITIVE_INFINITY, Number.NEGATIVE_INFINITY,
            new Number(NaN), new Number(+0), new Number( -0), new Number(0), new Number(1),
            new Number(10.0), new Number(10.1),
            new Number(Number.MAX_VALUE), new Number(Number.MIN_VALUE), new Number(Number.NaN),
            new Number(Number.POSITIVE_INFINITY), new Number(Number.NEGATIVE_INFINITY),
            "", "hello", "hel" + "lo", "+0", "-0", "0", "1", "10.0", "10.1",
            new String(""), new String("hello"), new String("he" + "llo"),
            new Object(), [1,2,3], new Object(), [1,2,3] , foo
          ];

function AsmModuleInt() {
    "use asm";

    function add(x,y) {
        x = x|0;
        y = y|0;
        return (x+y)|0;
    }
    function sub(x,y) {
        x = x|0;
        y = y|0;
        return (x-y)|0;
    }

    function div(x,y) {
        x = x|0;
        y = y|0;
        return ((x|0)/(y|0))|0;
    }

    function rem(x,y) {
        x = x|0;
        y = y|0;
        return ((x|0)%(y|0))|0;
    }

    function addfix(x) {
        x = x|0;
        return (x+1048575)|0;
    }
    function subfix(x) {
        x = x|0;
        return (x-1048575)|0;
    }
    function mulfix(x) {
        x = x|0;
        return (x*1048575)|0;
    }

    function divfix(x) {
        x = x|0;
        return ((x|0)/1048575)|0;
    }

    function remfix(x) {
        x = x|0;
        return ((x|0)%1048575)|0;
    }

    return {
        add : add,
        sub : sub,
        div : div,
        rem : rem,
        addfix : addfix,
        subfix : subfix,
        mulfix : mulfix,
        divfix : divfix,
        remfix : remfix,
    };
}

function AsmModuleUInt() {
    "use asm";

    function add(x,y) {
        x = x|0;
        y = y|0;
        return ((x>>>0)+(y>>>0))|0;
    }
    function sub(x,y) {
        x = x|0;
        y = y|0;
        return ((x>>>0)-(y>>>0))|0;
    }

    function div(x,y) {
        x = x|0;
        y = y|0;
        return ((x>>>0)/(y>>>0))|0;
    }

    function rem(x,y) {
        x = x|0;
        y = y|0;
        return ((x>>>0)%(y>>>0))|0;
    }


    function addfix(x) {
        x = x|0;
        return (x+4294967295)|0;
    }
    function subfix(x) {
        x = x|0;
        return (x-4294967295)|0;
    }

    function mulfix(x) {
        x = x|0;
        return ((x>>>0)*1048575)|0;
    }

    function divfix(x) {
        x = x|0;
        return ((x>>>0)/4294967295)|0;
    }

    function remfix(x) {
        x = x|0;
        return ((x>>>0)%4294967295)|0;
    }

    return {
        add : add,
        sub : sub,
        div : div,
        rem : rem,
        addfix : addfix,
        subfix : subfix,
        mulfix : mulfix,
        divfix : divfix,
        remfix : remfix,
    };
}

function AsmModuleDouble() {
    "use asm";

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

    function rem(x,y) {
        x = +x;
        y = +y;
        return +(x%y);
    }

    return {
        add : add,
        sub : sub,
        mul : mul,
        div : div,
        rem : rem,
    };
}

var asmModuleInt = AsmModuleInt();     // produces AOT-compiled version
var asmModuleUInt = AsmModuleUInt();     // produces AOT-compiled version
var asmModuleDouble = AsmModuleDouble();     // produces AOT-compiled version
print("Math for ints");
for (var i=0; i<all.length; ++i) {
    for (var j=0; j<all.length; ++j) {
        print("i  a["+i+"](" + all[i] +") + a["+j+"]("+all[j]+") = " + (asmModuleInt.add(all[i],all[j])));
        print("i  a["+i+"](" + all[i] +") - a["+j+"]("+all[j]+") = " + (asmModuleInt.sub(all[i],all[j])));
        print("i  a["+i+"](" + all[i] +") / a["+j+"]("+all[j]+") = " + (asmModuleInt.div(all[i],all[j])));
        print("i  a["+i+"](" + all[i] +") % a["+j+"]("+all[j]+") = " + (asmModuleInt.rem(all[i],all[j])));
    }
    print("i  a["+i+"](" + all[i] +") + 1048575 = " + (asmModuleInt.addfix(all[i])));
    print("i  a["+i+"](" + all[i] +") - 1048575 = " + (asmModuleInt.subfix(all[i])));
    print("i  a["+i+"](" + all[i] +") * 1048575 = " + (asmModuleInt.mulfix(all[i])));
    print("i  a["+i+"](" + all[i] +") / 1048575 = " + (asmModuleInt.divfix(all[i])));
    print("i  a["+i+"](" + all[i] +") % 1048575 = " + (asmModuleInt.remfix(all[i])));
}

print("Math for unsigned ints");
for (var i=0; i<all.length; ++i) {
    for (var j=0; j<all.length; ++j) {
        print("ui a["+i+"](" + all[i] +") + a["+j+"]("+all[j]+") = " + (asmModuleUInt.add(all[i],all[j])));
        print("ui a["+i+"](" + all[i] +") - a["+j+"]("+all[j]+") = " + (asmModuleUInt.sub(all[i],all[j])));
        print("ui a["+i+"](" + all[i] +") / a["+j+"]("+all[j]+") = " + (asmModuleUInt.div(all[i],all[j])));
        print("ui a["+i+"](" + all[i] +") % a["+j+"]("+all[j]+") = " + (asmModuleUInt.rem(all[i],all[j])));
    }
    print("ui  a["+i+"](" + all[i] +") + 4294967295 = " + (asmModuleUInt.addfix(all[i])));
    print("ui  a["+i+"](" + all[i] +") - 4294967295 = " + (asmModuleUInt.subfix(all[i])));
    print("ui  a["+i+"](" + all[i] +") * 1048575 = "    + (asmModuleUInt.mulfix(all[i])));
    print("ui  a["+i+"](" + all[i] +") / 4294967295 = " + (asmModuleUInt.divfix(all[i])));
    print("ui  a["+i+"](" + all[i] +") % 4294967295 = " + (asmModuleUInt.remfix(all[i])));
}

print("Math for Ddoubles");
for (var i=0; i<all.length; ++i) {
    for (var j=0; j<all.length; ++j) {
        print("f  a["+i+"](" + all[i] +") + a["+j+"]("+all[j]+") = " + (asmModuleDouble.add(all[i],all[j])));
        print("f  a["+i+"](" + all[i] +") - a["+j+"]("+all[j]+") = " + (asmModuleDouble.sub(all[i],all[j])));
        print("f  a["+i+"](" + all[i] +") * a["+j+"]("+all[j]+") = " + (asmModuleDouble.mul(all[i],all[j])));
        print("f  a["+i+"](" + all[i] +") / a["+j+"]("+all[j]+") = " + (asmModuleDouble.div(all[i],all[j])));
        print("f  a["+i+"](" + all[i] +") % a["+j+"]("+all[j]+") = " + (asmModuleDouble.rem(all[i],all[j])));
    }
}

