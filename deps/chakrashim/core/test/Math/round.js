//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

var FAILED = false;

// check rounding of NaN
checknan(Math.round(NaN), "Math.round(NaN)");
checknan(Math.round(Math.asin(2.0)), "Math.round(Math.asin(2.0))");

// check rounding of Infinity
check(Infinity, Math.round(Infinity), "Math.round(Infinity)");
check(-Infinity, Math.round(-Infinity), "Math.round(-Infinity)");

// check positive and negative 0
//check(0, Math.round(+0), "Math.round(+0)");
check(-0, Math.round(-0), "Math.round(-0)");

// check various values between 0 and 0.5
check(1, Math.round(4.9999999999999994000e-001), "round largest value < 0.5"); // for ES5 the result is 0
check(0, Math.round(4.9999999999999989000e-001), "round 2nd largest value < 0.5");
check(0, Math.round(4.9406564584124654000e-324), "round smallest value > 0");
check(0, Math.round(9.8813129168249309000e-324), "round 2nd smallest value > 0");
for(var i = 0.001; i < 0.5; i += 0.001)
{
    check(0, Math.round(i), "round " + i);
}

// check various values between -0.5 and 0
checkisnegativezero(Math.round(-4.9406564584124654000e-324), "round most positive value < 0");
checkisnegativezero(Math.round(-9.8813129168249309000e-324), "round 2nd most positive value < 0");
checkisnegativezero(Math.round(-4.9999999999999994000e-001), "round most negative value > -0.5");
checkisnegativezero(Math.round(-4.9999999999999989000e-001), "round 2nd most negative value > -0.5");
checkisnegativezero(Math.round(-0), "round -0 should be -0");

for(var i = -0.001; i > -0.5; i -= 0.001)
{
    checkisnegativezero(Math.round(i), "round " + i);
}

// check various integers
check(1, Math.round(1), "round 1");
check(2, Math.round(2), "round 2");
check(-1, Math.round(-1), "round -1");
check(-2, Math.round(-2), "round -2");
check(4294967295, Math.round(4294967295), "round 4294967295");
check(4294967296, Math.round(4294967296), "round 4294967296");
check(-4294967296, Math.round(-4294967296), "round -4294967296");
for(var i = 1000; i < 398519; i += 179)
{
    check(i, Math.round(i), "round " + i);
}
for(var i = 0.001; i <= 0.5; i += 0.001)
{
    check(1, Math.round(0.5 + i), "round " + (0.5+i));
}
for(var i = -0.001; i >= -0.5; i -= 0.001)
{
    check(-1, Math.round(-0.5 + i), "round " + (-0.5+i));
}

// check I + 0.5
check(1, Math.round(0.5), "round 0.5");
check(2, Math.round(1.5), "round 1.5");
check(3, Math.round(2.5), "round 2.5");
check(4294967296, Math.round(4294967295 + 0.5), "round 4294967295.5");
for(var i = -100000; i <= 100000; i += 100)
{
    check(i+1, Math.round(i + 0.5), "round " + (i+0.5));
}

// miscellaneous other real numbers
check(30593859183, Math.round(30593859183.3915898), "round a double with high precision");
check(1, Math.round(5.0000000000000011000e-001), "round smallest value > 0.5");
check(1, Math.round(5.0000000000000022000e-001), "round 2nd smallest value > 0.5");
check(1.7976931348623157000e+308, Math.round(1.7976931348623157000e+308), "round largest number < Infinity");
check(1.7976931348623155000e+308, Math.round(1.7976931348623155000e+308), "round 2nd largest number < Infinity");
check(-1.7976931348623157000e+308, Math.round(-1.7976931348623157000e+308), "round least positive number > -Infinity");
check(-1.7976931348623155000e+308, Math.round(-1.7976931348623155000e+308), "round 2nd least positive number > -Infinity");

// values around INT_MIN and INT_MAX for amd64 (Bug 179932)
function foo(b)
{
    var round = Math.round(b);

    if(round <= 2147483647)
    {
        FAILED = true;
    }
}
foo(2147483648);

function bar(b)
{
    var round = Math.round(b);

    if(round >= -2147483648)
    {
        FAILED = true;
    }
}
bar(-2147483649);

if (!FAILED)
{
    WScript.Echo("Passed");
}

function check(x, y, str)
{
    if(x != y)
    {
        FAILED = true;
        WScript.Echo("fail: " + str);
    }
}
function checkisnegativezero(x, str)
{
    // this is a quick way to check if a number is -0
    if(x != 0 || 1/x >= 0)
    {
        FAILED = true;
        WScript.Echo("fail: " + str);
    }
}
function checknan(x, str)
{
    if(!isNaN(x))
    {
        FAILED = true;
        WScript.Echo("fail: " + str);
    }
}
