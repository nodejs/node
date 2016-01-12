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

function test(s1, s2, b1)
{
    // Force float-pref
    s1 += 0.1;
    s2 += 0.1;
    b1 += 0.1;

    var r = false;

    // Test 1
    Count++;  r = false;
    if (s1 == b1) {
        r = true;
    }
    Check("s1 == b1", r, false);

    // Test 2
    Count++; r = false;
    if (s1 != b1)
    {
        r = true;
    }
    Check("s1 != b1", r, true);

    // Test 3
    Count++; r = false;
    if (s1 <= b1) {
        r = true;
    }
    Check("s1 <= b1", r, true);

    // Test 4
    Count++; r = false;
    if (s1 < b1) {
        r = true;
    }
    Check("s1 < b1", r, true);

    // Test 5
    Count++; r = false;
    if (s1 >= b1) {
        r = true;
    }
    Check("s1 >= b1", r, false);

    // Test 6
    Count++; r = false;
    if (s1 > b1) {
        r = true;
    }
    Check("s1 > b1", r, false);

    // Test 7
    Count++; r = false;
    if (s1 == s2)
    {
        r = true;
    }
    Check("s1 == s2", r, true);

    // Test 8
    Count++; r = false;
    if (s1 != s2) {
        r = true;
    }
    Check("s1 != s2", r, false);

    // Test 9
    Count++; r = false;
    if (s1 <= s2) {
        r = true;
    }
    Check("s1 <= s2", r, true);

    // Test 10
    Count++; r = false;
    if (s1 < s2) {
        r = true;
    }
    Check("s1 < s2", r, false);

    // Test 11
    Count++; r = false;
    if (s1 >= s2) {
        r = true;
    }
    Check("s1 >= s2", r, true);

    // Test 12
    Count++; r = false;
    if (s1 > s2) {
        r = true;
    }
    Check("s1 > s2", r, false);


    // Test 13
    Count++;
    Check("s1 == b1", s1 == b1, false);

    // Test 14
    Count++;
    Check("s1 != b1", s1 != b1, true);

    // Test 15
    Count++;
    Check("s1 <= b1", s1 <= b1, true);

    // Test 16
    Count++;
    Check("s1 < b1", s1 < b1, true);

    // Test 17
    Count++;
    Check("s1 >= b1", s1 >= b1, false);

    // Test 18
    Count++;
    Check("s1 > b1", s1 > b1, false);

    // Test 19
    Count++;
    Check("s1 == s2", s1 == s2, true);

    // Test 20
    Count++;
    Check("s1 != s2", s1 != s2, false);

    // Test 21
    Count++;
    Check("s1 <= s2", s1 <= s2, true);

    // Test 22
    Count++;
    Check("s1 < s2", s1 < s2, false);

    // Test 23
    Count++;
    Check("s1 >= s2", s1 >= s2, true);

    // Test 24
    Count++;
    Check("s1 > s2", s1 > s2, false);


    // Test 25
    Count++;  r = false;
    if (s1 === b1) {
        r = true;
    }
    Check("s1 === b1", r, false);

    // Test 26
    Count++; r = false;
    if (s1 !== b1)
    {
        r = true;
    }
    Check("s1 !== b1", r, true);

    // Test 27
    Count++; r = false;
    if (s1 === s2)
    {
        r = true;
    }
    Check("s1 === s2", r, true);

    // Test 28
    Count++; r = false;
    if (s1 !== s2) {
        r = true;
    }
    Check("s1 !== s2", r, false);

    // Test 29
    Count++;
    Check("s1 === b1", s1 === b1, false);

    // Test 30
    Count++;
    Check("s1 !== b1", s1 !== b1, true);

    // Test 31
    Count++;
    Check("s1 === s2", s1 === s2, true);

    // Test 32
    Count++;
    Check("s1 !== s2", s1 !== s2, false);



    if (!Failed)
    {
        print("Passed");
    }

}


test(1.1, 1.1, 2.1);
