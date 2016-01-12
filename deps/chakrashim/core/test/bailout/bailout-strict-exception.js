//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function test0() {
    'use strict';
    try {
        var obj0 = {};
        var ary = new Array(10);
        obj0.length = 1;
        var __loopvar3 = 0;
        while ((1) && __loopvar3 < 3) {
            __loopvar3++;
            ary.length = -804513990;
        }
        //Snippet 3: fewer arguments than formal parameters
        obj0.length = (function (x, y, z, w, r) {
            e *= obj0.prop0;
            var temp = x + y + z + w + r;
            return temp + ary[(1)];
        })(1, 1, 1);
    }
    catch(e) {
        WScript.Echo(e);
    }
};

// generate profile
test0();

// run JITted code
test0();

var shouldBailout = false;
function test1() {
    'use strict';
    try {
        var obj0 = {};
        var obj1 = {};
        var func2 = function (p0) {
            switch ((d)) {
                case 1:
                    break;
                case (a--):
                    break;
                default:
                    obj1.prop0 -= 1;
                    break;
                case 1:
                    break;
                case 1:
                    break;
            }
            (shouldBailout ? (Object.defineProperty(obj0, 'prop0', { writable: false, enumerable: true, configurable: true }), 1) : 1);
        }
        obj1.method0 = func2;
        var a = 1;
        var d = -27;
        obj1.method0();
        var __loopvar0 = 0;
        do {
            __loopvar0++;
        } while (((obj0.length & (shouldBailout ? (obj0.prop0 = { valueOf: function () { WScript.Echo('obj0.prop0 valueOf'); return 3; } }, 1) : 1))) && __loopvar0 < 3)
    }
    catch (e) {
        WScript.Echo(e);
    }
    WScript.Echo("obj1.prop0 = " + (obj1.prop0 | 0));
};

// generate profile
test1();

// run JITted code
test1();

// run code with bailouts enabled
shouldBailout = true;
test1();
