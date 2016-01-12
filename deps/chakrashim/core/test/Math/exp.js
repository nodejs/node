//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

// interesting floating point limits
checkNaN(NaN);
check(1, +0);
check(1, -0.0);
check(+Infinity, +Infinity);
check(0, -Infinity);
check(Math.E * Math.E * Math.E, 3);

if(!isNaN(Math.exp()))
{
    WScript.Echo("error: Math.exp() is not NaN");
}

WScript.Echo("done");

function check(result, n) {
    var rs = Math.exp(n);

    if (isNaN(result) || isNaN(n) ||
        Math.abs(rs - result) > 0.00000000001) {
        WScript.Echo("exp(" + n + ") != " + result);
        WScript.Echo(" wrong result is exp(" + n + ") = " + rs);
    }
}
function checkNaN(x) {
    var rs = Math.exp(x);
    if (!isNaN(rs)) {
        WScript.Echo("exp(" + x + ") !=  NaN");
        WScript.Echo(" wrong result is exp(" + x + ") = " + rs);
    }
}
