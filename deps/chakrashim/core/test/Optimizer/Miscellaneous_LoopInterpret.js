//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function test0() {
    var loopInvariant = 8;
    var func1 = function() {
        var __loopvar3 = loopInvariant;
    };
    var __loopvar0 = loopInvariant;
    while(test0.caller) {
        var __loopvar3 = 4;
        for(var _i in arguments) {
            __loopvar3 = loopInvariant;
            do {
                __loopvar3++;
                _test0_nonexistent;
            } while(typeof _test0_nonexistent && loopInvariant);
        }
    }
}
test0();
test0();

function test1() {
    var loopInvariant = 2;
    var arrObj0 = {};
    var IntArr0 = [
            -1,
            763598293,
            7527353389438789000
    ];
    var IntArr1 = [];
    var obj1;
    (function() {
        var __loopvar2 = loopInvariant;
        do {
            if(IntArr1[__loopvar2] != 1.1) {
                IntArr0[__loopvar2];
            } else {
                obj1 = 1;
                arrObj0(IntArr1[__loopvar2]);
            }
        } while(arrObj0.prop0);
    })();
}
test1();
test1();

function test2() {
    var FloatArr0 = [];
    var g = 1;
    var __loopvar2, loopInvariant, a, func0, obj0;
    for(var _strvar25 in FloatArr0) {
        if(typeof _strvar25 === 'string' && _strvar25.indexOf('method') != -1) {
            continue;
        }
        if(__loopvar0++ > 3) {
            break;
        }
        for(var __loopvar2 = loopInvariant - 3; __loopvar2 < loopInvariant && g < 1; __loopvar2++, g++) {
        }
        var strvar9 = a.concat(func0.call(obj0, --g, 1));
        strvar9 = strvar9.substring(strvar9.length / 1, strvar9.length / 3);
    }
}
test2();
test2();

function test3() {
    for(var i = 0; i < 2; ++i) {
        (1190787012.1 << 2147483648, {}) / test3a.call(2147483648);
    }

    function test3a() { }
}
test3();

function test4() {
    var loopInvariant = 3;
    var GiantPrintArray = [];
    var obj1 = {};
    obj1.prop1 = 1;
    var __loopvar0 = loopInvariant;
    for(; ;) {
        __loopvar0++;
        if(__loopvar0 == loopInvariant + 4) {
            break;
        }
        var v6 = 0;
        var v7 = obj1.prop1;
        while(v6 < 5) {
            v7 = obj1.prop1;
            v7 -= v6;
            v6++;
        }
        obj1.prop4 = v7;
        GiantPrintArray.push();
    }
    WScript.Echo("test4: " + obj1.prop4);
}
test4();
test4();
test4();

(function test5Runner() {
    var n = { iS: function(t) { try { } catch(ex) { } return false; } };
    function test5(e, t) {
        var r = e.split("#")[0].split("?")[1] || "",
            i = r.split("&"),
            s, o, u, a, f = n.iS(t),
            l = f ? "" : {};
        if(!r) return l;
        for(u = 0; u < i.length; u++) {
            s = i[u], o = s.split("=");
            if(!s) continue;
            if(!t) l[o[0]] = o[1] || "";
            else if(f && o[0] === t) l = o[1];
            else
                for(a = 0; a < t.length; a++) o[0] === t[a] && (l[o[0]] = o[1] || "")
        }
        return l
    }
    test5("http://a.b.c/?a=0&b=1&c=2", ['a', 'b']);
    test5("http://a.b.c/?a=0&b=1&c=2", ['a', 'b']);
})();

function test6() {
    var n = 10;
    var o = {};
    for(; o.length;) {
        var i = n - 10;
        do {
            i += 3;
            if(i > n + 3)
                break;
            o.length;
        } while(false);
        if(n === n + 3)
            break;
    }
}
test6();
test6();

function test7(i) {
    while(i &= 3) {
        while(++i) {
            if(1) {
                break;
            }
        }
    }
}
test7(1);

function test8() {
    for(var i = 0; i < 2; ++i) {
        (2147483650 >>> i) & 1;
        test8a.call(2147483650);
    }

    function test8a() { };
}
test8();
