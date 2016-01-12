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

function throwExceptionWithCatch()
{
    try
    {
        throwException();
    }
    catch(e)
    {
        Dump(TrimStackTracePath(e.stack) || e);
    }
}

var errorObject;
function throwException()
{
    errorObject = new Error("this is my error");
    throw errorObject;
}

function runtest(max, catchException)
{
    var helper = function(i)
    {
        return function(j)
        {
            if (j == 0)
            {
                return catchException == undefined ? throwExceptionWithCatch() : throwException();
            }
            that["function" + (j-1)](j-1);
        }
    }
    var that = new Object();
    var i = 0;
    for (i = 0; i < max; i++)
    {
        that["function" + i] = helper(i);
    }
    that["function" + (max-1)](max-1);
}

if (this.WScript && this.WScript.Arguments && this.WScript.LoadScriptFile("TrimStackTracePath.js"))
{
    if (this.WScript.Arguments[0] == "runTest")
    {
        runtest(this.WScript.Arguments[1]);
    }
}
