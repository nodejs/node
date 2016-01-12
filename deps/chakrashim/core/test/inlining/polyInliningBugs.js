//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

var __counter = 0;
function test0() {
    __counter++;
    var obj0 = {};
    var obj1 = {};
    var func1 = function () {
    };
    var func2 = function () {
    };
    obj0.method1 = func1;
    obj1.method1 = func2;
    protoObj0 = Object.create(obj0);
    protoObj1 = Object.create(obj1);
    obj0 = protoObj1;
    var __loopvar3 = 0;
    for (; __loopvar3 < 3; __loopvar3++) {
        (function () {
            for (var v2518 = 0; v2518 < arguments.length; ++v2518) {
                var uniqobj5 = [
                        protoObj0,
                        obj0
                    ];
                uniqobj5[__counter % uniqobj5.length].method1();
            }
        }(1));
    }
}
test0();
test0();
test0();
WScript.Echo("PASSED\n");
