//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function test0() {
    var func6 = function () {
        var __loopvar2 = 0;
        while ((arguments.callee[(1)]) && __loopvar2 < 3) {
            __loopvar2;
        }
    }
    var __loopvar1 = 0;
    do {
        __loopvar1;
    } while ((func6()) && __loopvar1 < 3)
    (function (argMath15) {
        func6();
    })(1);
};
test0();
WScript.Echo('pass');