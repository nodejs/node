//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function assert(actual, expected)
{
    if (actual !== expected)
   {
        throw new Error("failed test Actual: " + actual + " Expected: " + expected);
   }
}

function test0(s1, s2) {
    for(var i = 0; i < 1; ++i)
        s2 += s1.charAt( s1 = "k" + s1);
    assert(s2, "u");
};

test0("u", "");
test0("u", "");
WScript.Echo("PASSED");