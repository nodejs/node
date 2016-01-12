//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

// -maxinterpretcount:1 -off:objtypespec
function test0(o2) {
    var o = {};
    var a = [1];
    var sum = a[0];
    sum += a[0];
    o.a = a;
    if(!o2)
        o.a = [];
    o2.b = a;
    var b = o.a;
    b[0] = 2;
    sum += b[0];
    return sum;
}
var o2 = {};
Object.defineProperty(
    o2,
    "b",
    {
        configurable: true,
        enumerable: true,
        set: function(a) {
            Object.defineProperty(
                a,
                "0",
                {
                    configurable: true,
                    enumerable: true,
                    writable: false,
                    value: 999
                });
        }
    });
WScript.Echo(test0({}));
WScript.Echo(test0(o2));

// -maxinterpretcount:1 -off:objtypespec
function test1() {
    test1a({ p: 2 }, { p2: 0 }, 0);
    var o = { p: 2 };
    var o2 = {};
    Object.defineProperty(
        o2,
        'p2',
        {
            configurable: true,
            enumerable: true,
            set: function() {
                o.p = 2;
            }
        });
    test1a(o, o2, 0);

    function test1a(o, o2, b) {
        o.p = true;
        if(b)
            o.p = true;
        o2.p2 = o2;
        return o.p >>> 2147483647;
    }
};
test1();
