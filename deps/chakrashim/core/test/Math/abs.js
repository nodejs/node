//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

// interesting floating point limits
checkNaN(NaN);
check(+0, +0);
check(+0, -0);
check(+Infinity, +Infinity);
check(+Infinity, -Infinity);

check(3.14, -3.14);
check(3.14, 3.14);
check(5, -5);
check(5, 5);

check(2147483647, 2147483647);  /* INT_MAX */
check(2147483648, -2147483648); /* INT_MIN */

if(!isNaN(Math.abs()))
{
    WScript.Echo("error: Math.abs() is not NaN");
}

WScript.Echo("done");

function check(result, n) {
    var rs = Math.abs(n);
    if (rs != result) {
        WScript.Echo("abs(" + n + ") != " + result);
        WScript.Echo(" wrong result is abs(" + n + ") = " + rs);
    }
}

function checkNaN(x) {
    var rs = Math.abs(x);
    if (!isNaN(rs)) {
        WScript.Echo("abs(" + x + ") !=  NaN");
        WScript.Echo(" wrong result is abs(" + x + ") = " + rs);
    }

}
