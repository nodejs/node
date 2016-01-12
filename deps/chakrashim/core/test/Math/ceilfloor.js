//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

// interesting floating point limits
check(NaN, NaN);
check(+0, +0);
check(-0, -0);
check(+Infinity, +Infinity);
check(-Infinity, -Infinity);

// values abs(x) < 1
check(-0, -4.9406564584124654000e-324);
check(-0, -9.8813129168249309000e-324);
check(-0, -0.5);
check(-0, -9.9999999999999989000e-001);
check(-0, -9.9999999999999978000e-001);
check(-1, -1);
check(1,   4.9406564584124654000e-324);
check(1,   9.8813129168249309000e-324);
check(1, 0.5);
check(1, 9.9999999999999989000e-001);
check(1, 9.9999999999999978000e-001);
check(1, 1);

// other interesting double values
var x = 1;
for(var i = 0; i < 50; ++i)
{
    check(x, x - 0.1);
    check(-x + 1, -x + 0.1);
    x = x * 2;
}
check(54, 53.7);
check(112233581321, 112233581320.001);

// values around the maximums
check(1.7976931348623157000e+308, 1.7976931348623157000e+308);
check(-1.7976931348623157000e+308, -1.7976931348623157000e+308)

// values around INT_MIN and INT_MAX for amd64 (Bug 179932)
function foo(b)
{
    //Its okay to check only for ceil as correctness tests for floor are already here and floor and ceil will have the same value for the parameter passed for this test
    var ceil = Math.ceil(b);

    if(ceil <= 2147483647)
        return "fail";

    return "pass";
}
WScript.Echo(foo(2147483648));

function bar(b)
{
    //Its okay to check only for ceil as correctness tests for floor are already here and floor and ceil will have the same value for the parameter passed for this test
    var ceil = Math.ceil(b);

    if(ceil >= -2147483648)
        return "fail";

    return "pass";
}
WScript.Echo(bar(-2147483649));

WScript.Echo("done");

function check(result, n)
{
    if(!isNaN(n))
    {
        if(Math.ceil(n) != result)
        {
            WScript.Echo("ceil(" + n + ") != " + result);
        }
        if(-Math.floor(-n) != result)
        {
            WScript.Echo("floor(" + (-n) + ") != " + (-result));
        }
    }
    else
    {
        if(!isNaN(Math.ceil(n)) || !isNaN(-Math.floor(-n)))
        {
            WScript.Echo("error with ceil/floor of NaNs");
        }
    }
}

Verify("Math.ceil around negative 0", -Infinity, 1/Math.ceil(-0.1));
Verify("Math.floor around negative 0", -Infinity, 1/Math.floor(-0));

function Verify(test, expected, actual) {
    if (expected === actual) {
        WScript.Echo("PASSED: " + test);
    }
    else {
        WScript.Echo("FAILED: " + test + " Expected:" + expected + "Actual:" + actual);
    }
}
