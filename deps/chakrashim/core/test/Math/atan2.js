//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

// interesting floating point limits

checkNaN(NaN, NaN);
checkNaN(2, NaN);
checkNaN(NaN, -3);

check((Math.PI) / 2, 3, +0);
check((Math.PI) / 2, 3, -0);

check(0, 0, 3);
check(0, 0, 0);
check(Math.PI, 0, -0);
check(Math.PI, 0, -2);

check(-0, -0, 3);
check(-0, -0, 0);
check(-Math.PI, -0, -0);
check(-Math.PI, -0, -2);

check(-(Math.PI) / 2, -3, +0);
check(-(Math.PI) / 2, -3, -0);

check(0, 3, +Infinity);
check((Math.PI), 3, -Infinity);

check(-0, -3, +Infinity);

check((Math.PI)/2, +Infinity, 3);
check(-(Math.PI) / 2, -Infinity, 3);
check((Math.PI) / 2, +Infinity, -3);
check(-(Math.PI) / 2, -Infinity, -3);


check((Math.PI) / 4, 5, 5.0);

if(!isNaN(Math.atan2()))
{
    WScript.Echo("error: Math.atan2() is not NaN");
}

WScript.Echo("done");

function check(result, y, x) {
    var res = Math.atan2(y, x);
    if (Math.abs(res - result) > 0.00000000001) {
        WScript.Echo("atan2(" + y + " , " + x + ") != " + result);
        WScript.Echo(" the wrong result is atan2(" + y + " , " + x + ") = " + res);
    }
}

function checkNaN(y, x) {
    var rs = Math.atan2(y, x);
    if (!isNaN(rs)) {
        WScript.Echo("atan2(" + y + " , " + x + ") !=  NaN");
        WScript.Echo(" wrong result is atan2(" + y + " , " + x + ") = " + rs);
    }


}