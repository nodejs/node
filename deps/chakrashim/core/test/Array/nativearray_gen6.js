//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function test0() {
    function leaf() { return 100; };
    var obj1 = {};
    var arrObj0 = {};
    var func0 = function (argMath0, argArr1, argObj2) {
        var __loopvar16 = 0;
        while ((1) && __loopvar16 < 3) {
            __loopvar16++;
            argArr1[((((leaf.call(argObj2) % (0 ? 2147483647 : -7.33527460009626E+18)) >= 0 ? (leaf.call(argObj2) % (0 ? 2147483647 : -7.33527460009626E+18)) : 0)) & 0XF)] = (--obj1.prop0);
            obj1.length *= argArr1[(16)];
        }
    }
    var ui16 = new Uint16Array(256);
    var intary = [4, 66, 767, -100, 0, 1213, 34, 42, 55, -123, 567, 77, -234, 88, 11, -66];
    var __loopvar1 = 0;
    for (var strvar0 in ui16) {
        if (strvar0.indexOf('method') != -1) continue;
        if (__loopvar1++ > 3) break;
        obj1.prop0 = 1;
        var __loopvar3 = 0;
        do {
            __loopvar3++;
            obj1.prop0 = func0.call(obj1, 1, intary, 1);
        } while ((1) && __loopvar3 < 3)
        intary[(18)] = (arrObj0.length--);
    }
};

// generate profile
test0();
test0();
test0();

// run JITted code
runningJITtedCode = true;
test0();
test0();
test0();

WScript.Echo('pass');
