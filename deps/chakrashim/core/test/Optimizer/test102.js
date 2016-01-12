//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

var shouldBailout = false;
function test0() {
    var obj1 = {};
    var arrObj0 = {};
    var ary = new Array(10);
    var ui16 = new Uint16Array(256);
    var c = 1;
    var f = 1;
    arrObj0.prop0 = -254;
    for(var __loopvar0 = 0; __loopvar0 < 3 && f < ((-arrObj0.prop0)) ; __loopvar0++ + f++) {
        for(var __loopvar1 = 0; ; __loopvar1++) {
            if(__loopvar1 > 3) break;
            var __loopvar4 = 0;
            while((1) && __loopvar4 < 3) {
                __loopvar4++;
                if(c) {
                    break;
                }
                var __loopvar5 = 0;
                while((1) && __loopvar5 < 3) {
                    __loopvar5++;
                    if(shouldBailout) {
                        func1 = obj0.method0;
                    }
                    obj1.prop1 = ui16[(1) & 255];
                }
            }
            obj0 = obj1;
            obj0.length = ary[((shouldBailout ? (ary[1] = "x") : undefined), 1)];
        }
    }
};
test0();
test0();
test0();
test0();
shouldBailout = true;
test0();

WScript.Echo("pass");
