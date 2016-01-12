//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

// interesting floating point limits
checkNaN(NaN);
check(+0, +0);
check(-0, -0.0);
check((Math.PI) / 2, +Infinity);
check(-(Math.PI) / 2, -Infinity);
check((Math.PI) / 4, 1);


if(!isNaN(Math.atan()))
{
    WScript.Echo("error: Math.atan() is not NaN");
}

WScript.Echo("done");

function check(result, n) {
    var res = Math.atan(n);
    if (Math.abs(res - result) > 0.00000000001) {
        WScript.Echo("atan(" + n + ") != " + result);
        WScript.Echo(" wrong result is atan(" + n + ") = " + res);
    }
}
function checkNaN(x) {
    var rs = Math.atan(x);
    if (!isNaN(rs)) {
        WScript.Echo("atan(" + x + ") !=  NaN");
        WScript.Echo(" wrong result is atan(" + x + ") = " + rs);
    }
}
