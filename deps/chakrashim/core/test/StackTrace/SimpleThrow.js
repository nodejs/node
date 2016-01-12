//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function Dump(output)
{
    if (this.WScript)
    {
        WScript.Echo(output);
    }
    else
    {
        alert(output);
    }
}

function throwException()
{
    try
    {
        BadType.someProperty = 0;
    }
    catch(e)
    {
        Dump(TrimStackTracePath(e.stack));
        Dump("");
    }
}

function throwExceptionWithFinally()
{
    try
    {
        BadTypeWithFinally.someProperty = 0;
    }
    catch(e)
    {
        Dump(TrimStackTracePath(e.stack));
        Dump("");
    }
    finally {} // Do nothing
}

function throwExceptionLineNumber()
{
    try
    {
        StricModeFunction();
    }
    catch(e)
    {
        Dump(TrimStackTracePath(e.stack));
    }
}

function StricModeFunction()
{
"use strict"
    this.nonExistentProperty = 1;
    
    if(1) {}
    
    WScript.Echo("foo");
}

function bar()
{
    throwException();
    throwExceptionWithFinally();
    throwExceptionLineNumber();
}

function foo()
{
    bar();
}

function runtest()
{
    foo();
}

if (this.WScript && this.WScript.Arguments && this.WScript.LoadScriptFile("TrimStackTracePath.js"))
{
    if (this.WScript.Arguments[0] == "runTest")
    {
        runtest();
    }
}
