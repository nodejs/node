//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

// interesting floating point limits
checkNaN(NaN);
check(+0, +0);
check(-0.0, -0.0);
checkNaN(+Infinity);
checkNaN(-Infinity);

check(1, (Math.PI) /4);


if(!isNaN(Math.tan()))
{
    WScript.Echo("error: Math.tan() is not NaN");
}

WScript.Echo("done");

function check(result, n) {
    var rs = Math.tan(n);
    if (Math.abs(rs - result) > 0.00000000001) {
        WScript.Echo("tan(" + n + ") != " + result);
        WScript.Echo(" wrong result is tan(" + n + ") = " + rs);
    }
}

function checkNaN(x) {
    var rs = Math.tan(x);
    if (!isNaN(rs)) {
        WScript.Echo("tan(" + x + ") !=  NaN");
        WScript.Echo(" wrong result is tan(" + x + ") = " + rs);
    }
}