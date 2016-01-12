//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function test(a, b, a) {
    var x = function () {
        eval('');
    };

    var success = true;
    if (a !== 3) {
        WScript.Echo("Failed: a !== 3, a: " + a);
        success = false;
    }
    if (b !== 2) {
        WScript.Echo("Failed: b !== 2, b: " + b);
        success = false;
    }
    return success;
}

if (test(1, 2, 3)) {
    WScript.Echo("Pass");
}
