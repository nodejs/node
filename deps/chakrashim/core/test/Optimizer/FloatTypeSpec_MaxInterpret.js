//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function test0(a) {
    if(a + 0.1 === 0 || a === 0)
        return true;
    return false;
}
WScript.Echo("test0: " + toSafeInt(test0(0.1)));
WScript.Echo("test0: " + toSafeInt(test0(false)));

function test1(a) {
    if(a + 0.1 === 0)
        return -1;
    return a;
}
WScript.Echo("test1: " + toSafeInt(test1(0.1)));
WScript.Echo("test1: " + toSafeInt(test1(false)));

function test2(a) {
    if(a + 0.1 === 0)
        a = 0.1;
    return a;
}
WScript.Echo("test2: " + toSafeInt(test2(0.1)));
WScript.Echo("test2: " + toSafeInt(test2(false)));

// -off:copyProp
function test3(a) {
    if(a + 0.1 === 0)
        return a;
    var b = a;
    if(b === 0)
        return a;
    return -1;
}
WScript.Echo("test3: " + toSafeInt(test3(0.1)));
WScript.Echo("test3: " + toSafeInt(test3(false)));

// -off:inline
function test4(a) {
    var b, c, d, r;
    if(a + 0.1 === 0)
        return a;
    b = a;
    if(b + 0.1 === 0)
        return b;
    c = b;
    if(c + 0.1 === 0)
        return c;
    for(var i = 0; i < 3; ++i) {
        d = c;
        c = b;
        b = a;
        if(d === 0)
            r = a;
        else
            r = -1;
        a = test3a(a);
        if(a + 0.1 === 0)
            return a;
    }
    return r;

    function test3a(a) { return a; }
}
WScript.Echo("test4: " + toSafeInt(test4(0.1)));
WScript.Echo("test4: " + toSafeInt(test4(false)));

// -off:copyProp
function test5(a) {
    if(a + 0.1 === 0)
        return a;
    var b = 0.1, c = 0.1, d = 0.1, e = 0.1, f = 0.1;
    for(var i = 0; i < 5; ++i) {
        e = d;
        c = b;
        b = a;
        d = c;
        f = e;
    }
    return f;
}
WScript.Echo("test5: " + toSafeInt(test5(0.1)));
WScript.Echo("test5: " + toSafeInt(test5(null)));

function test6(a, b) {
    var c = a + 1 !== NaN ? a : a + 1;
    return c + b;
};
WScript.Echo("test6: " + test6(0.1, 0));
WScript.Echo("test6: " + test6(undefined, ""));

function test7() {
    var obj0 = {};
    var obj1 = {};
    var func0 = function (p0, p1, p2, p3) {
        function func3(p0, p1, p2) {
            LABEL0:
            switch(p0) {
                case (p0 /= obj1.prop2, 1.1):
                    break;
                case p3:
                    obj0.prop2 = (obj1.prop1--);
                    break;
            }
        }
        var __loopvar3 = 0;
        while(Math.atan(func3())) {
            if(__loopvar3 > 3) break;
            __loopvar3++;
        }
    }
    obj1.prop1 = 1;
    obj1.prop2 = { prop1: 1, prop0: 1 };
    func0()
    return obj1.prop1;
};
WScript.Echo("test7: " + test7());
WScript.Echo("test7: " + test7());

function test8() {
    var a = x, x = Math.pow(x, "");
    return a;

};
WScript.Echo("test8: " + test8());
WScript.Echo("test8: " + test8());

function toSafeInt(n) {
    if(typeof (n) !== "number")
        return n;
    return Math.round(n * 10);
}
