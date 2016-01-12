//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

// interesting floating point limits
checkNaN(NaN);
checkNaN(-Infinity);
checkNaN(-0.1);
check(+0, +0);
check(-0.0, -0.0);
check(+Infinity, +Infinity);
check(5, 25);


if(!isNaN(Math.sqrt()))
{
    WScript.Echo("error: Math.sqrt() is not NaN");
}

WScript.Echo("done");

function check(result, n) {
    var rs = Math.sqrt(n);
    if (Math.abs(rs - result) > 0.00000000001) {
        WScript.Echo("sqrt(" + n + ") != " + result);
        WScript.Echo(" wrong result is sqrt(" + n + ") = " + rs);
    }
}
function checkNaN(x) {
    var rs = Math.sqrt(x);
    if (!isNaN(rs)) {
        WScript.Echo("sqrt(" + x + ") !=  NaN");
        WScript.Echo(" wrong result is sqrt(" + x + ") = " + rs);
    }
}
