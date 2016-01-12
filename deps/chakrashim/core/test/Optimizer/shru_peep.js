//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

var Failed = 0;

function FAILED()
{
    Failed++;
}

function test(a, b)
{
    if (((a|0)>>>0) > ((b|0)>>>0))
    return true;
    else
    return false;
}

function foo()
{
    return 1;
}

function bar()
{
    return -1;
}

function test2(a, f)
{
    if (((a|0)>>>0) > ((f()|0)>>>0))
    return true;
    else
    return false;
}

for (var i = 0; i < 50; i++)
{
    if (!test(-1, 1))
    FAILED();
    if (!test2(-1, foo))
    FAILED();
}

if (test(1,-1))
    FAILED();

if (!test(-1,(-1>>>0)+10))
    FAILED();

// Force bailout
if (test2(1,bar))
    FAILED();

if (Failed == 0)
    WScript.Echo("Passed");
else
    WScript.Echo("FAILED");
