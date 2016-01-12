//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function test0() {
    var obj0 = {};
    var obj1 = {};
    var arrObj0 = {};
    var floatary = [-1.5, -0.5, -0, 1.5, 12.987, 12.123, 100.33, 8.8, 5.5, 44.66, 42.24, 124.07, -0.99, 56.65, 42.24];
    var a = 1;
    var d = 1;
    obj1.prop1 = -1740804447.9;
    arrObj0.prop1 = 20;
    function bar4(argMath8) {
        arrObj0.prop0 >>= obj1.prop1;
    }
    obj0._x = { a: 1, b: { n: 1 } }
    for (var v406513 = 0; v406513 < 3; v406513++) {
        // CSE when expressions present on LHS.

        function _csetest(_x, _y) {
            _x.prop1 = _y;
            return _x;
        }

        _csetest({ "prop1": 1 }, (-383063099.9 ? arrObj0.prop1 : 1)).prop2 = (-383063099.9 ? arrObj0.prop1 : 1);

        floatary[(-383063099.9 ? arrObj0.prop1 : 1)] = 1 + d;
        var v406514 = floatary[(-383063099.9 ? arrObj0.prop1 : 1)] + floatary[(-383063099.9 ? arrObj0.prop1 : 1)];
        a += obj0._x.b.n + obj0._x.a;
    }
    WScript.Echo("a = " + a);

    WScript.Echo("obj1.prop1 = " + (obj1.prop1 | 0));
};

// generate profile
test0();
test0();
test0();
test0();
test0();

// run JITted code
runningJITtedCode = true;
test0();
test0();
test0();
test0();
test0();

