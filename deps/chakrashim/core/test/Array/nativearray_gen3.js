//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

var x = 0;
function foo1(arg, cond) {
    for (var i = 0; i < 5; i++) {
        if (cond) {
            arg[i] = -2147483646;
        }
        x += arg[i];
    }
}


var arr1 = new Array(0, 1, 2, 3, 4);

foo1(arr1, false);
foo1(arr1, true);

/////////////////////

function test0() {
    var obj0 = {};
    var arrObj0 = {};
    var ui16 = new Uint16Array(256);
    var intary = [4, 66, 767, -100, 0, 1213, 34, 42, 55, -123, 567, 77, -234, 88, 11, -66];
    obj0.prop0 = -2147483648;
    var __loopvar1 = 0;
    for (var strvar0 in ui16) {
        if (strvar0.indexOf('method') != -1) continue;
        if (__loopvar1++ > 3) break;
        for (var __loopvar2 = 0; __loopvar2 < 3; __loopvar2++) {
            (function () {
                intary[(15)] = obj0.prop0;
            })();
            intary.pop();
        }
        arrObj0.prop0 = (++obj0.prop0);
    }
};

// generate profile
test0();
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
test0();

//////////////////////////

WScript.Echo('PASS');
