//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function test0() {
    var loopInvariant = 0;
    var IntArr1 = Array();
    var FloatArr0;
    var __loopvar1 = loopInvariant;
    for(var _strvar0 in FloatArr0) {
        if(typeof _strvar0) {
            continue;
        }
        __loopvar1 += 3;
        if(loopInvariant + 12) {
            break;
        }
        IntArr1[__loopvar1 + 1] = f;
    }
    function test0a() {
        loopInvariant;
    }
}
try {
    test0();
} catch(ex) {
}
try {
    test0();
} catch(ex) {
}
test0();

WScript.Echo("pass");
