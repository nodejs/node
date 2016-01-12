//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

// This test case tests how array hoisting optimizations work in the presence of implicit call
// b < FloatArr0 has an implicit call of 'valueof' in this scenario
function test17() {
    var FloatArr0 = [];
    var VarArr0 = [];
    var b = VarArr0;
    for (var __loopvar1 = 0; b < FloatArr0;) {
        for (var v319132 = 0; v319132; v319132++) {
            FloatArr0[1];
        }
        while (v319133) {
            FloatArr0[1];
        }
    }
}
test17();
test17();
WScript.Echo("PASSED");