//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function print(s) {
    if (typeof(WScript) == "undefined")
        document.write(s + "<br/>");
    else
        WScript.Echo(s);
}

var Count = 0;
var Failed = 0;

function Check(str, result, expected)
{
    if (result != expected)
    {
        print("Test #"+Count+" failed. <"+str+"> Expected "+expected);
        Failed++;
    }
}

function test()
{
    var x = NaN + 0.5;
    var r = false;

    // Test 1
    Count++; r = false;
    if (x == x) {
        r = true;
    }
    Check("x == x", r, false);

    // Test 2
    Count++; r = false;
    if (x != x) {
        r = true;
    }
    Check("x != x", r, true);

    // Test 3
    Count++; r = false;
    if (x <= x) {
        r = true;
    }
    Check("x <= x", r, false);

    // Test 4
    Count++; r = false;
    if (x < x) {
        r = true;
    }
    Check("x < x", r, false);

    // Test 5
    Count++; r = false;
    if (x >= x) {
        r = true;
    }
    Check("x >= x", r, false);

    // Test 6
    Count++; r = false;
    if (x > x) {
        r = true;
    }
    Check("x > x", r, false);

    // Test 7
    Count++;
    Check("x == x", x == x, false);

    // Test 8
    Count++;
    Check("x != x", x != x, true);

    // Test 9
    Count++;
    Check("x <= x", x <= x, false);

    // Test 10
    Count++;
    Check("x < x", x < x, false);

    // Test 11
    Count++;
    Check("x >= x", x >= x, false);

    // Test 12
    Count++;
    Check("x > x", x > x, false);

    // Test 13
    Count++; r = false;
    if (x === x) {
        r = true;
    }
    Check("x === x", r, false);

    // Test 14
    Count++; r = false;
    if (x !== x) {
        r = true;
    }
    Check("x !== x", r, true);


    if (!Failed)
    {
        print("Passed");
    }
}


test();
