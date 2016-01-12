//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

var failed = 0;
function runtest(name, func, throwException)
{
    try
    {
        func();
        if (throwException)
        {
            WScript.Echo(name + ": Test failed, unexpected no exception thrown");
            failed++;
        }
        else
        {
            WScript.Echo(name + ": Test passed, expected no exception thrown");
        }
    }
    catch (e)
    {
        if (!throwException || (e.name != "TypeError" && e.name != "ReferenceError"))
        {
            WScript.Echo(name + ": test failed, unexpected " + e.name + "-" + e.message);
            failed++;
        }
        else
        {
            WScript.Echo(name + ": Test passed, expected " + e.name + "-" + e.message);
        }
    }
}

function assert(cond)
{
    if (!cond)
    {
        throw new Error("AssertFailed");
    }
}
//-------------------------------------------------------------
// Test 0 - check stuff
//-------------------------------------------------------------

function test0()
{
    assert(eval.prototype == undefined);
}

//-------------------------------------------------
// Test 1 - throw, new built in function eval()
//-------------------------------------------------
function test1()
{
    new eval();
}

runtest("test0", test0, false);
runtest("test1", test1, true);

if (failed == 0)
{
    WScript.Echo("Passed");
}
