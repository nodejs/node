//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

// No arguments
check(Infinity, Math.min(), "min()");

// NaN
check(NaN, Math.min(NaN), "min(NaN)");

// Other non-numbers
check(NaN, Math.min({}), "min({})");
check(NaN, Math.min({}, {}), "min({}, {})");
check(NaN, Math.min({}, 42), "min({}, 42)");

// null
check(0, Math.min(null), "min(null)");

// +0 / -0
check(0, Math.min(+0,-0), "min(+0,-0)");
check(0, Math.min(-0,+0), "min(-0,+0)");
check(0, Math.min(+0, 5, -0), "min(+0, 5, -0)");
check(0, Math.min(-0, 5, +0), "min(-0, 5, +0)");

// Length property
check(2, Math.min.length, "min.length");

var values = [0, -0, -1, 1, -2, 2, 3.14159, 1000.123, 0x1fffffff, -0x20000000, Infinity, -Infinity];

for(var i=0; i < values.length; ++i)
{
    // single value
    check(values[i], Math.min(values[i]), "min(" + values[i] + ")");

    // Infinitys
    check(-Infinity, Math.min(values[i],  -Infinity), "min(" + values[i] + ", -Infinity)");
    check(values[i], Math.min(values[i], +Infinity), "min(" + values[i] + ", +Infinity)");
}

// check min is the first/last value in the array
check(-5, Math.min(-5, 1, 2, 3, 4), "min is the first value");
check(-5, Math.min(4, 1, 2, 3, -5), "min is the last value");
check(-57000.4, Math.min(1.3, 1, -57000.4, 3, 4), "min is the first value");

// check max with 50 nearby double values

check(3.09815037958998160000e+007,
    Math.min(
    3.0981503795899998000e+007,
    3.0981503795899995000e+007,
    3.0981503795899991000e+007,
    3.0981503795899987000e+007,
    3.0981503795899983000e+007,
    3.0981503795899980000e+007,
    3.0981503795899976000e+007,
    3.0981503795899972000e+007,
    3.0981503795899969000e+007,
    3.0981503795899965000e+007,
    3.0981503795899961000e+007,
    3.0981503795899957000e+007,
    3.0981503795899954000e+007,
    3.0981503795899950000e+007,
    3.0981503795899946000e+007,
    3.0981503795899943000e+007,
    3.0981503795899939000e+007,
    3.0981503795899935000e+007,
    3.0981503795899931000e+007,
    3.0981503795899928000e+007,
    3.0981503795899924000e+007,
    3.0981503795899920000e+007,
    3.0981503795899916000e+007,
    3.0981503795899913000e+007,
    3.0981503795899909000e+007,
    3.0981503795899905000e+007,
    3.0981503795899902000e+007,
    3.0981503795899898000e+007,
    3.0981503795899894000e+007,
    3.0981503795899890000e+007,
    3.0981503795899887000e+007,
    3.0981503795899883000e+007,
    3.0981503795899879000e+007,
    3.0981503795899875000e+007,
    3.0981503795899872000e+007,
    3.0981503795899868000e+007,
    3.0981503795899864000e+007,
    3.0981503795899861000e+007,
    3.0981503795899857000e+007,
    3.0981503795899853000e+007,
    3.0981503795899849000e+007,
    3.0981503795899846000e+007,
    3.0981503795899842000e+007,
    3.0981503795899838000e+007,
    3.0981503795899834000e+007,
    3.0981503795899831000e+007,
    3.0981503795899827000e+007,
    3.0981503795899823000e+007,
    3.0981503795899820000e+007,
    3.0981503795899816000e+007
), "many values");

// special cases on -0.0, infinity, etc

WScript.Echo("Math.min(+0.0, -0.0) " + Math.min(+0.0, -0.0));

if (1 / Math.min(+0.0, -0.0) < 0) {
    WScript.Echo("Check (1 / Math.min(+0.0, -0.0) < 0)  - true ");
}
else {

    WScript.Echo("Check (1 / Math.min(+0.0, -0.0) < 0)  - false ")
}

check(Number.NEGATIVE_INFINITY, Math.min(5, Number.NEGATIVE_INFINITY), "min the negative infinity");
check(5, Math.min(5, Number.POSITIVE_INFINITY), "min is 5");
check(Number.NEGATIVE_INFINITY, Math.min(Number.NEGATIVE_INFINITY, Number.POSITIVE_INFINITY), "min is the negative infinity");
WScript.Echo("done");

function check(actual, expected, str)
{
    if (isNaN(expected))
    {
        if (actual.toString() !== expected.toString())
        {
            WScript.Echo("fail: " + str);
        }
    }
    else
    {
        if (actual !== expected)
        {
            WScript.Echo("fail: " + str);
        }
    }
}

//negative zero
function foo(a,b)
{
    var c = Math.min(a,b);
    return 1/c;
}
WScript.Echo(foo(-0,0));
WScript.Echo(foo(-0,0));
WScript.Echo(foo(0,-0));
WScript.Echo(foo(-0,-0));

function BLUE143505(a,b)
{
    var c;
    if ((Math.min(-4, undefined))) {
        c = false;
    }
    return c;
};
WScript.Echo(BLUE143505());
WScript.Echo(BLUE143505());
