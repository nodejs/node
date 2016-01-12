//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

// interesting floating point limits
checkNaN(NaN);
check(1, +0);
check(1, -0.0);
checkNaN(+Infinity);
checkNaN(-Infinity);

check(0, (Math.PI) /2);
check(0.5 , (Math.PI) / 3);



if(!isNaN(Math.cos()))
{
    WScript.Echo("error: Math.cos() is not NaN");
}

WScript.Echo("done");

function check(result, n) {
    var rs = Math.cos(n);
    if (Math.abs(rs - result) > 0.00000000001) {
        WScript.Echo("cos(" + n + ") != " + result);
        WScript.Echo(" wrong result is cos(" + n + ") = " + rs);
    }
}
function checkNaN(x) {
    var rs = Math.cos(x);
    if (!isNaN(rs)) {
        WScript.Echo("cos(" + x + ") !=  NaN");
        WScript.Echo(" wrong result is cos(" + x + ") = " + rs);
    }
}
