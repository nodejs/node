//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

// interesting floating point limits
checkNaN(NaN);
checkNaN(5.1);
checkNaN(-2);

check(+0, +0);
check(-0, -0.0);
check((Math.PI) / 2, 1);
check((Math.PI) / 6, 0.5);
checkNaN(+Infinity);
checkNaN(-Infinity);


if(!isNaN(Math.asin()))
{
    WScript.Echo("error: Math.asin() is not NaN");
}

WScript.Echo("done");

function check(result, n) {
    var rs = Math.asin(n);
    if (Math.abs(rs - result) > 0.00000000001) {
        WScript.Echo("asin(" + n + ") != " + result);
        WScript.Echo(" wrong result is asin(" + n + ") = " + rs);
    }
}

function checkNaN(x) {
    var rs = Math.asin(x);
    if (!isNaN(rs)) {
        WScript.Echo("asin(" + x + ") !=  NaN");
        WScript.Echo(" wrong result is asin(" + x + ") = " + rs);
    }
}