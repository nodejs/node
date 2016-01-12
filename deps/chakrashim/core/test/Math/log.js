//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

// interesting floating point limits
checkNaN(NaN);
checkNaN(-4);
check(-Infinity, +0);
check(-Infinity, -0.0);
check(0, 1);
check(+Infinity, +Infinity);
check(3, Math.E*Math.E*Math.E);

if(!isNaN(Math.log()))
{
    WScript.Echo("error: Math.log() is not NaN");
}

WScript.Echo("done");

function check(result, n) {
    var rs = Math.log(n);
    if ( Math.abs(rs - result) > 0.00000000001) {
        WScript.Echo("log(" + n + ") != " + result);
        WScript.Echo(" wrong result is log(" + n + ") = " + rs);
    }
}

function checkNaN(x) {
    var rs = Math.log(x);
    if (!isNaN(rs)) {
        WScript.Echo("log(" + x + ") !=  NaN");
        WScript.Echo(" wrong result is log(" + x + ") = " + rs);
    }
}

