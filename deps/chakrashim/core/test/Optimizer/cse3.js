//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

﻿//Switches:  -macinterpretcount:1 -loopinterpretcount:1 -bgjit-
var GiantPrintArray = [];
function test0() {
    var obj0 = {};
    var arrObj0 = {};
    var func0 = function () {
    }
    var func2 = function () {
        GiantPrintArray.push("hello");
    }
    obj0.method0 = func0;
    Object.prototype.method0 = func2;
    var ui32 = new Uint32Array(256);
    var __loopvar0 = 0;
    for (var strvar23 in ui32) {
        if (__loopvar0++ > 3) break;
        function func8() { }
        arrObj0.method0(1, 1, 1, 1);
    }
    var __loopvar0 = 0;
    for (var strvar23 in ui32) {
        if (__loopvar0++ > 3) break;
        var __loopvar2 = 0;
        do {
            __loopvar2++;
            (obj0 > (new obj0.method0()))
        } while (__loopvar2 < 3)
        (function () {
            eval("")
        })();
        var __loopvar2 = 0;
        do {
            __loopvar2++;
            // Simple Javascript OO pattern
            var a = (function () {
            })(new obj0.method0(new obj0.method0()));
            obj0;

        } while (__loopvar2 < 3)
    }
    WScript.Echo(GiantPrintArray.length);
};
// generate profile
test0();

