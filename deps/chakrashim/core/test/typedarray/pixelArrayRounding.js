//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function test0() {
    var ui8 = new Uint8Array(256);
    var cpa8 = ui8;
    var g = 0;
    var h = 0.5;
    var i = 0;
    var total = 0;
    while (g < 10) {
        // Make sure we round ties the same way between the interpreter and the JIT-ed code.
        cpa8[i] = g + h;
        WScript.Echo("cpa8[" + i + "] = " + cpa8[i++]);
        g++;
    }
    return total;
};

test0();

test0();
