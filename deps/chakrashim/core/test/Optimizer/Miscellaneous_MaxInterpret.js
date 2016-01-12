//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

var profile, result;
var a = 0;

function toSafeInt(n) {
    return Math.round(Math.round(n * 10) / 10);
}

function test0() {
    for(var i = 0; i < 2; ++i) {
        if(1) do {
            return 0;
        } while(1);
    }
};
test0();
test0();

function test1() {
    var d = 1;
    var g = 1.1;
    for(var i = 0; i < 2; ++i) {
        d ^= g >= 0 ? g : 0;
        for(var j = 0; j < 2; ++j)
            d = g;
        g & 255;
    }

    function test1a() { d; }
};
test1();
test1();

function test2() {
    var obj0 = {};
    var obj1 = {};
    var func0 = function(p0, p1) {
        this.method0 = func0;
    }
    obj0.method0 = func0;
    var f = 1;
    for(var __loopvar0 = 0; __loopvar0 < 3; ++__loopvar0) {
        (function(p0, p1, p2, p3) {
            "use strict";
            obj1 = (new obj0.method0(1, 1, 1, 1, 1, 1));
        })(1);
        obj0 = obj1;
        var litObj3 = { prop0: ((obj0.prop0++) - Math.pow((1383554054 + -22), (obj0.prop0 ^= 1))), prop1: obj0.prop1, prop2: 1, prop3: 1, prop4: 1, prop5: 1, prop6: (--f), prop7: 1.1, prop8: 1 };
    }
};
test2();
test2();

var shouldBailout = false;
function test3() {
    for(var i = 0; i < 3; ++i) {
        if(- ~2147483647, 1, (this.prop0--), 1, 1, ((shouldBailout ? func2 = leaf : 1), 1));
    }
};
test3();
test3();

function test4() {
    var o = profile ? createFloat64Array() : [1.1];
    var p = "0";
    o[p] += o[p];
    return o[p];

    function createFloat64Array() {
        var a = new Float64Array(1);
        a[0] = 1.1;
        return a;
    }
}
profile = true;
test4();
profile = false;
result = test4();
new RegExp("test4").exec("test4"); // overwrite the stack to invalidate any numbers that were allocated on the stack
WScript.Echo("test4: " + result);

function test5() {
    var obj0 = {};
    var func1 = function(p0, p1, p2) {
    }
    obj0.method0 = func1;
    var b = 1;
    var e = 1730745725.1;
    (function() {
        var __loopvar1 = 0;
        while(((e >>= 714649918.1)) && __loopvar1 < 3) {
            __loopvar1++;
            b /= obj0.method0(1, 1, -2071119430.9, 1);
            e = 1949149674.1;
            WScript.Echo("test5: good");
            return 1;
        }
        var litObj3 = { prop0: 1, prop1: 4.1219102077814E+17, prop2: 1, prop3: 1, prop4: -8.45710040200392E+18, prop5: 1, prop6: 1, prop7: 1, prop8: 1 };
        obj0.method0((this.prop0 * -3.51338110666416E+18), obj0.prop1);
        if(-634731549.9) {
            if((obj0.prop1 <= 1)) {
                var __loopvar6 = 0;
                while(((Function("") instanceof Boolean)) && __loopvar6 < 3) {
                    __loopvar6++;
                    obj0.prop0 = (1, obj1.prop1, obj1.prop1);
                    a = obj1;
                    let a;
                }
            }
            else {
                e = -1152902162.9;
            }
        }
        else {
            WScript.Echo("obj0.prop1 = " + (obj0.prop1 | 0));
        }
        WScript.Echo("obj1.prop1 = " + (obj1.prop1 | 0));
    })();
};
test5();
test5();

function test6() {
    var a = 1;
    var b = 8.58155630194204E+18;
    var i = 0;
    do {
        for(var j = 0; j < 2; ++j) {
            b /= 1;
            a = 1 % a;
        }
    } while(++i < 2);
};
test6();
test6();

// - Profile data says that 'd' is likely float
// - Due to 'd = 1', we have a tagged int for 'd'
// - In the loop prepass, the source value should not be changed because the value is not precise as we're still in the prepass
function test7() {
    var a, d;
    var o = {};
    for(var i = 0; d = a * 1 && i < 2; ++i);
    for(var i = 0; i < 2 && o.p < 1; i++ + o.p++) {
        d = 1;
        for(var j = 0; j < 2; ++j)
            d %= 1;
        a = 1;
    }
    return d;

    function test11a() { d; }
};
test7();
test7();

// - 'b' is a slot variable and is treated as a field
// - At 'b = i', the value of 'i' gets transferred to 'b'
// - At 'c = c && b', in the block that handles 'c = b', it is copy-propped as 'c = i'
// - Even though 'i' is invariant with respect to the inner loop, if 'c = i' is hoisted out of the inner loop as 'c2 = i' and
//   inside the loop, 'c = c2', then 'c2' should not be made the sym store of the value of 'i' because after the merge caused by
//   '&&', 'c2' won't be live anymore (since we don't hoist 'c2' into every preceding block, and only to the landing pad)
// - At the final 'c = b', assuming the above problem manifests, the value of 'b' will have a sym store 'c2', which is not live
//   anymore. So, the value's sym store is set to 'b', giving ownership of the value to 'b'. This is less than ideal because 'i'
//   actually owns the value.
// - At this point, because 'b' now owns the value, and profile data says it is likely float (because of the first assignment to
//   'b'), we go ahead and change the value type to likely float. Since 'b' doesn't actually own the value, that effectively
//   changes the value type of 'i' to likely float as well.
function test8(a) {
    var b = -2147483648 % 1;
    var c = 0;
    if(a) {
        for(var i = 0; i < 2; i++) {
            for(var j = 0; j < 2; ++j) {
                b = i;
                c = c && b;
                c = b;
            }
        }
    }
    return b;

    function test0a() { b; }
};
test8();
test8();

function test9() {
    var d = 1;
    var __loopvar0 = 0;
    do {
        __loopvar0++;
        d = (-2 / ((--d) == 0 ? 1 : (--d)));
    } while((1) && __loopvar0 < 3)
    return d | 0;
};
WScript.Echo("test9: " + test9());
WScript.Echo("test9: " + test9());

// Implicit calls should be disabled at the load of 'a.p' with flow-based array check hoisting
function test10(a, b) {
    var sum = a[0];
    sum += a.p;
    var a2 = a;
    a = b;
    a2[0] = ++b;
    sum += a2[0];
    sum += a + b;
    return sum;
}
var test10_a = [1];
test10_a.p = 2;
WScript.Echo("test10: " + test10(test10_a, 3));
Object.defineProperty(
    test10_a,
    "p",
    {
        configurable: true,
        enumerable: true,
        get: function() {
            Object.defineProperty(
                test10_a,
                "0",
                {
                    configurable: true,
                    enumerable: true,
                    get: function() { return 5; },
                    set: function() { }
                });
            return 2;
        }
    });
WScript.Echo("test10: " + test10(test10_a, 3));

function test11() {
    var obj0 = {};
    var a = 1;
    var l = 1;
    a = (-2061493789 << 1);
    l = a;
    obj0.prop0 = a;
    a = ((~1) * (-obj0.prop0));
    a ^= obj0.prop0;
    return l | 0;
};
test11();
test11();

function test12() {
    var c = 1;
    var f = 1;
    a = 21414358.1;
    while(f < 1)
        c();
    while(a %= 1) {
        c = Math.abs(c) % 4;
        c = "substring" + c + "bug";
        c = c.substring(3, 11);
        switch(c) {
            case "string0b":
                break;
            case "string1b":
                break;
            case "string2b":
                break;
            case "string3b":
                break;
        };
        a = 1;
    }
};
test12();
test12();

function test13(a, rid, q) {
    for(var i = 0; i < q.length; i++) {
        if(q[i][0] == rid) {
            for(var j = 1; j < q[i].length; j++) {
                if(q[i][j] == a) {
                    if(q[i].length == 2) {
                        q.splice(i, 1);
                    }
                    else {
                        q[i].splice(j, 1);
                    }
                    return;
                }
            }
        }
    }
}
(function test13Runner() {
    var q = [];
    for(var i = 0; i < 4; ++i)
        q.push([0, 1, 2, 3]);
    test13(1, 0, q);
    test13(1, 0, q);
})();

function test14(a, i, o) {
    a[i] = 1;
    o.p;
    return a[i];
}
(function test14Runner() {
    var a = [1];
    var o = {};
    WScript.Echo("test14: " + test14(a, 0, o));
    Object.defineProperty(
        o,
        "p",
        {
            configurable: true,
            enumerable: true,
            get: function() {
                Object.defineProperty(
                    a,
                    "0",
                    {
                        configurable: true,
                        enumerable: true,
                        get: function() {
                            return 2;
                        }
                    });
            }
        });
    WScript.Echo("test14: " + test14(a, 0, o));
})();

function test15(a, o) {
    a[0] = 1;
    o.p;
    return a[0];
}
(function test15Runner() {
    var a = [1];
    var o = {};
    WScript.Echo("test15: " + test15(a, o));
    Object.defineProperty(
        o,
        "p",
        {
            configurable: true,
            enumerable: true,
            get: function() {
                Object.defineProperty(
                    a,
                    "0",
                    {
                        configurable: true,
                        enumerable: true,
                        get: function() {
                            return 2;
                        }
                    });
            }
        });
    WScript.Echo("test15: " + test15(a, o));
})();

(function test16Runner() {
    var obj0;
    function test16() {
        var obj1 = {};
        var arrObj0 = {};
        var func1 = function(argStr0, argObj1, argMath2) {
            eval("'use strict';");
        }
        var ui8 = new Uint8Array(256);
        var __loopvar0 = 0;
        do {
            __loopvar0++;
            var __loopvar1 = 0;
            while((1) && __loopvar1 < 3) {
                __loopvar1++;
            }
            obj0 = obj1;
        } while((1) && __loopvar0 < 3)
        if((arrObj0.length ? 1 : obj1.prop0)) {
        }
        11
    };
    test16();
    test16();
})();

function test17() {
    var x = 0;
    (function() {
        (function foo(y) { y >> null; })(x);
    })();
};
test17();
test17();

function test18(o) {
    function test18a(a, o) {
        var b = a + 1;
        var c = a + (o | 0);
        return b ^ c;
    }
    WScript.Echo("test18: " + test18a(0x7fffffff, o));
}
test18(1);
test18(
    {
        valueOf: function() {
            WScript.Echo("test18: valueOf");
            return 2;
        }
    });

function test20() {
    var a = new Array(10);
    var o = {};
    o.p = ~-2147483648;
    a[0] = o.p;
    return a[0];
};
test20();
test20();

(function test21Runner() {
    function test21(o) {
        var a = 0.1;
        var b = 1.1;
        var c = 2.1;
        a += 0.1;
        c += 0.1;
        var d = 0;
        for(var i = 0; i < 4; ++i) {
            d = a + 0.1;
            b += 0.1;
            a = b;
            b = c;
            c = o;
            if(!o)
                b = 6.1;
        }
        return d;
    }
    var o = { valueOf: function() { return 9.1; } };
    WScript.Echo("test21: " + toSafeInt(test21(o)));
    WScript.Echo("test21: " + toSafeInt(test21(o)));
})();

function test22() {
    var a = 1.5;
    var b = 1;
    for(var i = 0; i < 2; ++i) {
        a = b;
        b = true;
    }
    WScript.Echo("test22: " + (a | 0));
};
test22();
test22();

(function test23Runner() {
    function test23(o) {
        var sum = 0;
        var a = o.a;
        sum += a[0];
        sum += o.b[0];
        for(var i = 1; i < 2; ++i)
            a[i] = 1.1;
        sum += a[1];
        sum += o.b[1];
        return sum;
    }
    var o = {
        a: [1],
        b: [2, 3]
    };
    WScript.Echo("test23: " + test23(o));
    o.a = [1];
    o.b = o.a;
    WScript.Echo("test23: " + test23(o));
})();

(function test24Runner() {
    function test24(a, profile) {
        var sum = 0;
        var o = {};
        o.a = a;
        for(var i = 0; i < 2; ++i) {
            if(profile)
                o.a[1] = null;
            else
                sum += o.a[0];
        }
        return sum;
    }
    var a = [1.1, 2.2];
    WScript.Echo("test24: " + test24(a, true));
    WScript.Echo("test24: " + test24(a, false));
})();

(function test25Runner() {
    function test25(a, b) {
        var sum = 0;
        sum += a[0];
        b[3] = 0;
        sum += a[2];
        return sum;
    }
    var a = [1, 2, 3];
    var b = {};
    WScript.Echo("test25: " + test25(a, b));
    a = [1];
    b = a;
    WScript.Echo("test25: " + test25(a, b));
})();

(function test26Runner() {
    function test26(a, b) {
        var c = a[0];
        b[0] = "x";
        WScript.Echo("test26: " + a[0]);
    };
    var a = [1];
    test26(a, {});
    test26(a, a);
})();

(function test27Runner() {
    function test27(o) {
        if(o) {
            o.p;
            return 1;
            o.p === o.p;
        }
        Math.log(1);
    }
    var o = { get p() { return 0; } };
    test27(o);
    test27(0);
})();

(function test28Runner() {
    function test28(a, profile) {
        var sum = 0;
        var o = {};
        o.a = a;
        for(var i = 0; i < 2; ++i) {
            for(var j = 0; j < 2; ++j) {
                if(profile)
                    o.a[1] = null;
                else
                    sum += o.a[0];
            }

            for(var j = 0; j < 2; ++j) {
                if(profile)
                    o.a[1] = null;
                else
                    sum += o.a[0];
            }
        }
        return sum;
    }
    var a = [1.1, 2.2];
    WScript.Echo("test28: " + Math.round(test28(a, true) * 10) / 10);
    WScript.Echo("test28: " + Math.round(test28(a, false) * 10) / 10);
})();

function test29() {
    var a0 = [];
    var a1 = [];
    for(var i = 0; i < 2; ++i) {
        a0.push(0);
        test29a(a1);
    }

    function test29a(a) {
        a[2] = 1.1;
        a[1];
    }
};
test29();
test29();

(function test30Runner() {
    function test30(a) {
        a[0];
        for(var i = 0; i < 2; ++i) {
            if(i === 0)
                a[2] = 0;
            else
                a.push(0);
        }
    };
    var a = [0];
    test30(a);
    test30(a);
})();

(function test31Runner() {
    function test31(a) {
        a[0];
        for(var i = 0; i < 2; ++i) {
            if(i === 0)
                a[2] = 0;
            else
                a.push(0);
        }
    };
    var a = [null];
    test31(a);
    test31(a);
})();

function test32(b, d) {
    var c = 1;
    do {
        var a = c;
        c = -b;
        ++b;
    } while(d);
    return a;
};
WScript.Echo("test32: " + test32(0, 0));
WScript.Echo("test32: " + test32(0, 0));

function test33(a) {
    return test33a(a);

    function test33a(a) {
        return a << this;
    }
};
test33(3);
test33({});

function test34() {
    var a = [0];
    var b = a;
    a = a.pop();
    b.push(b[0]);
    a = b;
    for(var i = 0; i < 1; ++i) {
        if(!b) {
            b = 1;
            break;
        }
        a[0];
    }
};
test34();
test34();

function test35() {
    for(var i = 0; i < 1; ++i)
        var VarArr0 = [null];
    for(var i = 0; i < 1; ++i)
        VarArr0[2] = 0;
}
test35();
test35();

function test36() {
    return Math.abs(-2147483648) & 1;
}
test36();
test36();

function test37() {
    var k = ~(2147483648 + (1.1 & 1));
    k -= ((2147483647 - (2147483648 + (1.1 & 1))) * -1073741824);
    k |= 0;
    return k;
}
WScript.Echo("test37: " + test37());
WScript.Echo("test37: " + test37());

(function test38Runner() {
    var x;
    function test38() {
        var z1;
        var u3056;
        (function() {
            with(u3056 += [z1]) x;
        })();
        (function() {
            for(qmsdtp = 0; qmsdtp < 16 && undefined && (y %= null) ; ++qmsdtp) {
                if(qmsdtp % 7 == 5) {
                } else {
                    with(1 ^ (/x/g.prototype.prototype)) { }
                }
            };;
        })();
    };
    test38();
    test38();
})();

(function test39Runner() {
    var shouldBailout = false;
    function test39() {
        var IntArr1 = [152];
        var FloatArr0 = [-870672233.9];
        var VarArr0 = [];
        var a = 1377203759;
        var h = -760994394;
        this.prop1 = -7.90411814882572E+18;
        var __loopvar1 = 0;
        do {
            __loopvar1++;
        } while(((IntArr1[(((((shouldBailout ? (IntArr1[(((VarArr0[(1)]) >= 0 ? (VarArr0[(1)]) : 0) & 0xF)] = 'x') : undefined), VarArr0[(1)]) >= 0 ? VarArr0[(1)] : 0)) & 0XF)] ? (FloatArr0[(((((shouldBailout ? (FloatArr0[((((this.prop1 /= h)) >= 0 ? ((this.prop1 /= h)) : 0) & 0xF)] = 'x') : undefined), (this.prop1 /= h)) >= 0 ? (this.prop1 /= h) : 0)) & 0XF)] - FloatArr0[(((((shouldBailout ? (FloatArr0[((((a++)) >= 0 ? ((a++)) : 0) & 0xF)] = 'x') : undefined), (a++)) >= 0 ? (a++) : 0)) & 0XF)]) : 1)) && __loopvar1 < 3)
        11
    };
    test39();
    shouldBailout = true;
    test39();
})();

function test40() {
    var p;
    for(var i = 0; i < 1; i++) {
        do {
            p = 1 ^ -2147483649;
            if(1 !== p)
                p -= 2147483647;
            else
                break;
        } while(false);
    }
    return p;
}
WScript.Echo("test40: " + test40());
WScript.Echo("test40: " + test40());

function test41() {
    for(var i = 124.90088411141187; i < 3; i++) {
        var a = [_test41_nonexistentRootVar];
        for(var j = 264.3966821487993; j < 4; ++j) {
            var a = [_test41_nonexistentRootVar];
            _test41_nonexistentRootVar = a[2];
        }
    }
}
test41();
test41();

function test42() {
    var a = [];
    a[1] = -3;
    a[2147483648] = -3;
    a[a.length] = -2;
    function test42a() {
        return a[2147483646];
    }
    while(test42a()) {
    }
    a.pop();
    while(a.pop());
    "" + [].slice();
}
test42();
test42();
test42();
test42();

function test43() {
    var a = new Int8Array();
    do {
        { };
        Array(a).shift()[0];
    } while(false);
}
test43();
test43();

function test44() {
    var obj1 = {};
    var test44a = function() {
        return protoObj1 < obj1 || 1 != 1;
    };
    var protoObj1 = Object(obj1);
    obj1.prop0 = 1;
    for(var i = 0; test44a() && i < 1; ++i);
}
test44();
test44();

function test45() {
    var arrObj0;
    var protoObj0 = {};
    protoObj0.prop0 = 1;
    var v15 = Array();
    protoObj0.prop0;
    if(v15 instanceof Date) {
        v15[protoObj0.prop0 && arrObj0.length];
    }
}
test45();
test45();

function test46() {
    var protoObj0 = Object();
    protoObj0.length = 100;
    1 >> -964252921 + protoObj0.length - ~1;
}
test46();
test46();

function test47() {
    var a = 0, sum = 0;
    for(var i = 0; i < 2; ++i) {
        sum -= a;
        a = {};
    }
    return sum;
}
test47();
test47();

(function test48Runner() {
    var ctxt = {
        getStackVar: function() { try { } catch(ex) { } }
    };
    function test48(oargs, sttop2) {
        for(var asis_from = oargs.length - 1; asis_from >= 0; asis_from--) {
            if(oargs[asis_from].containsFunctionCall()) {
                break;
            }
        }
        asis_from++;
        var args = [];
        for(var i = 0; i < asis_from; i++) {
            args[i] = ctxt.getStackVar(sttop2 + i);
        }
        for(; i < oargs.length; i++) {
            args[i] = oargs[i];
        }
    }
    var args = [];
    test48(args, 0);
    test48(args, 0);
})();

function test49() {
    var obj0 = {};
    var arrObj0 = {};
    obj0.method1 = function() {
        while(arrObj0[{} + 0] || (arguments[{}] = 'x', arrObj0[{} + 0])) {
        }
    };
    arrObj0.method0 = obj0.method1;
    arrObj0[0] = -238193011.9;
    arrObj0.method0();
    "" + [];
}
test49();
test49();

function test50(a) {
    var index = -6;
    for(var i = 0; i < 1; ++i) {
        var index2 = index;
        index = index2;
        a[index];
    }
}
test50([]);
test50([]);

function test51(v1, n) {
    var v2 = 0;
    var v3 = -6;
    for(var i = 0; i < n; ++i) {
        v2 = v3;
        v1[v3];
        v3 = v2;
    }
}
test51([null], 0);
test51([null], 1);

(function test52Runner() {
    function test52(a) {
        var sum = 0;
        var j = 1;
        for(var i = 0; i < 12; ++i) {
            if(j == 11)
                j = 0;
            else
                ++j;
            sum += a[j];
        }
        return sum;
    }
    var a = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12];
    test52(a);
    test52(a);
})();

(function test53Runner() {
    function test53(a, j) {
        var sum = 0;
        for(var i = 0; i < 12; ++i) {
            if(j == 11)
                j = 0;
            else
                ++j;
            sum += a[j];
        }
        return sum;
    }
    var a = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12];
    test53(a, 1);
    test53(a, 1);
})();

function test54() {
    var a = 1420236100;
    for(var i = 0; a += 27, i < 1; ++i) {
    }
}
test54();
test54();

function test55(a, b) {
    b >>= 0;
    return a >>> b--;
}
WScript.Echo("test55: " + test55(-1, 0));
WScript.Echo("test55: " + test55(-1, 0));

function test56() {
    if('a' ? null : undefined) {
        if(this)
            test56a();
    }

    function test56a() { }
}
test56();
test56();

function test57(a) {
    var b = (a + 1) | 0;
    var c = a + 1;
    return c - b;
}
WScript.Echo("test57: " + test57(0x7fffffff));
WScript.Echo("test57: " + test57(0x7fffffff));

function test58(a) {
    return 1 / (a * -1 && test58a());

    function test58a() {
        try { } catch(ex) { }
        return 1;
    }
}
WScript.Echo("test58: " + test58(0));
WScript.Echo("test58: " + test58(0));

function test59() {
    var f64 = new Float64Array(256);
    if(!f64.length) {
        while(f64[120]) {
        }
    }
}
test59();
test59();

function test60(a) {
    var sum = 0;
    var j = 2;
    for(var i = 0; i < 2; ++i) {
        j -= j;
        sum += a[j];
    }
    return sum;
}
test60([1]);
test60([1]);

function test61(arr, a) {
    var v7 = 0;
    var v8 = 0;
    do {
        v8 = v7;
        v7 = arr[0];
        if(v8) {
            break;
        }
        ++a;
    } while(!test61);
    return a;
}
WScript.Echo("test61: " + test61([], 0));
WScript.Echo("test61: " + test61([], 0));

(function test62Runner() {
    var boo = false;
    function test62() {
        var n = boo ? 1 : 2;
        var a = [1, 1];
        var i = n - 2, j = i, k = i;
        var sum = 0;
        while(sum += a[j]) {
            ++i;
            if(i >= n)
                break;
            sum += a[k];
            ++j;
            ++k;
        }
    }
    test62();
    test62();
})();

function test63(a, b) {
    a |= 0;
    b |= 0;
    if(a >= b) {
        if(a + 1 === 0x3fffffff + 1)
            return a + 1;
    }
    return 0;
}
test63();
test63();

function test64(a) {
    a |= 0;
    if(a < 0)
        a = 0;
    return a + 0x7fffffff;
}
test64(0);
test64(0);

(function test65Runner() {
    function test65(o, s) {
        var a = o[s];
        o.a = a;
        var b;
        var sum = 0;
        for(var i = 0; i < 2; ++i) {
            sum += a.p;
            b = o.a;
            sum += a[i];
        }
        return sum + (b ? 1 : 0);
    }
    var o = { a: [1, 2] };
    o.a.p = 3;
    test65(o, "a");
    test65(o, "a");
})();

function test66() {
    var a = test66a(0);
    var b = test66a();
    return a + b;

    function test66a() {
        return test66a.arguments.length;
    }
}
WScript.Echo("test66: " + test66());
WScript.Echo("test66: " + test66());
