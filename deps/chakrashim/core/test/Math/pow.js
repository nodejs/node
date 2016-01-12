//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

// interesting floating point limits
checkNaN(NaN, NaN);
checkNaN(Infinity, NaN);
checkNaN(-Infinity, NaN);
checkNaN(0, NaN);
checkNaN(-0, NaN);
checkNaN(3, NaN);
checkNaN(-3, NaN);

check(1, NaN, 0);
check(1, Infinity, 0);
check(1, -Infinity, 0);
check(1, 0, 0);
check(1, -0, 0);
check(1, 3, 0);
check(1, -3, 0);

check(1, NaN, -0);
check(1, Infinity, -0);
check(1, -Infinity, -0);
check(1, 0, -0);
check(1, -0, -0);
check(1, 3, -0);
check(1, -3, -0);

checkNaN(NaN, 3);
checkNaN(NaN, Infinity);
checkNaN(NaN, -Infinity);

check(Infinity, +1.1, Infinity);
check(Infinity, -1.1, Infinity);

check(0, +1.1, -Infinity);
check(0, -1.1, -Infinity);

checkNaN(+1, Infinity);
checkNaN(-1, Infinity);

checkNaN(+1, -Infinity);
checkNaN(-1, -Infinity);

check(0, +0.9, Infinity);
check(0, -0.9, Infinity);

check(Infinity, +0.9, -Infinity);
check(Infinity, -0.9, -Infinity);

check(Infinity, Infinity, 0.1);
check(+0, Infinity, -0.1);

check(-Infinity, -Infinity, 3);
check(+Infinity, -Infinity, 4);

check(-0, -Infinity, -3);
check(+0, -Infinity, -4);

check(0, 0, 0.1);

check(+Infinity, +0, -0.1);

check(-0.0, -0, +3);
check(+0.0, -0, +4);
check(-Infinity, -0, -3);
check(+Infinity, -0, -4);

checkNaN(-3, 3.3);

check(25, 5, 2);

check(25, -5, 2);
check(1/25, -5, -2);

if(!isNaN(Math.pow()))
{
    WScript.Echo("error: Math.pow() is not NaN");
}
WScript.Echo("done");

function check(result, x, y) {
    var rs = Math.pow(x, y);
    if ( Math.abs(rs - result) > 0.00000000001) {
        WScript.Echo("pow(" + x + " , " + y + ") != " + result);
        WScript.Echo(" wrong result is pow(" + x + " , " + y + ") = " + rs);
    }
}

function checkNaN(x, y) {
    var rs = Math.pow(x, y);
    if (!isNaN(rs)) {
        WScript.Echo("pow(" + x + " , " + y + ") !=  NaN" );
        WScript.Echo(" wrong result is pow(" + x + " , " + y + ") = " + rs);
    }


}
