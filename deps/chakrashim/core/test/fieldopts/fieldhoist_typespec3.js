//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function test0() {
    var obj0 = {};
    var obj1 = {};
    obj1.prop0 = 2;
    test0a();
    var __loopvar0 = 0;
    do {
        __loopvar0++;
        obj0.prop0 = obj1.prop0 * obj1.prop0;
        obj1 = obj0;
    } while (obj1.prop0 && __loopvar0 < 2)
    WScript.Echo("obj1.prop0 = " + (obj1.prop0 | 0));

    function test0a() { }
};

// generate profile
test0();

// run JITted code
test0();
