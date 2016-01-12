//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

// No arguments
check(-Infinity, Math.max(), "max()");

// NaN
check(NaN, Math.max(NaN), "max(NaN)");

// Other non-numbers
check(NaN, Math.max({}), "max({})");
check(NaN, Math.max({}, {}), "max({}, {})");
check(NaN, Math.max({}, 42), "max({}, 42)");

// null
check(0, Math.max(null), "max(null)");

// +0 / -0
check(0, Math.max(+0,-0), "max(+0,-0)");
check(0, Math.max(-0,+0), "max(-0,+0)");
check(0, Math.max(+0, -5, -0), "max(+0, -5, -0)");
check(0, Math.max(-0, -5, +0), "max(-0, -5, +0)");

// Length property
check(2, Math.max.length, "max.length");

var values = [0, -0, -1, 1, -2, 2, 3.14159, 1000.123, 0x1fffffff, -0x20000000, Infinity, -Infinity];

for(var i=0; i < values.length; ++i)
{
    // single value
    check(values[i], Math.max(values[i]), "max(" + values[i] + ")");

    // Infinitys
    check(Infinity, Math.max(values[i],  Infinity), "max(" + values[i] + ",  Infinity)");
    check(values[i], Math.max(values[i], -Infinity), "max(" + values[i] + ", -Infinity)");
}

// check max is the first/last value in the array
check(5, Math.max(5, 1, 2, 3, 4), "max is the first value");
check(5, Math.max(4, 1, 2, 3, 5), "max is the last value");
check(57000.4, Math.max(1.3, 1, 57000.4, 3, 4), "max is the first value");

// check max with 50 nearby double values

check(3.0981503795899998000e+007,
    Math.max(
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

WScript.Echo(Number.POSITIVE_INFINITY);
WScript.Echo(Number.NEGATIVE_INFINITY);
var opD = +0.0;
var onD = -0.0;
var op = 1 / Number.POSITIVE_INFINITY
var on = 1 / Number.NEGATIVE_INFINITY;
var infp = 1 / op;
var infn = 1 / on;
var infpD = 1 / (+0.0);
var infnD = 1 / (-0.0);
WScript.Echo("opD = " + opD + " , onD = " + onD + " op = " + op + " , on = " + on + " , infp = " + infp + " , infn = " + infn + " , infpD = " + infpD + " , infnD = " + infnD);

WScript.Echo("Math.max(+0.0, -0.0) " + Math.max(+0.0, -0.0));

if (1 / Math.max(+0.0, -0.0) < 0) {
    WScript.Echo("Check (1 / Math.max(+0.0, -0.0) < 0)  - true ");
}
else {

    WScript.Echo("Check (1 / Math.max(+0.0, -0.0) < 0)  - false ")
}

check(5, Math.max(5, Number.NEGATIVE_INFINITY), "max is not the negative infinity");
check(Number.POSITIVE_INFINITY, Math.max(5, Number.POSITIVE_INFINITY), "max is the positive infinity");
check(Number.POSITIVE_INFINITY, Math.max(Number.NEGATIVE_INFINITY, Number.POSITIVE_INFINITY), "max is the positive infinity");

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
    var c = Math.max(a,b);
    return 1/c;
}
WScript.Echo(foo(-0,0));
WScript.Echo(foo(-0,0));
WScript.Echo(foo(0,-0));
WScript.Echo(foo(-0,-0));

function BLUE143505(a,b)
{
    var c;
    if ((Math.max(-4, undefined))) {
        c = false;
    }
    return c;
};
WScript.Echo(BLUE143505());
WScript.Echo(BLUE143505());

function findMaxInArray()
{
  var arr = new Array(3);
  prop1 = 0;
  arr[1] = 1;
  arr[0] = 1;
  WScript.Echo(Math.max.apply(Math, arr));
  Object.prototype[2] = prop1;
}
findMaxInArray();
findMaxInArray();
