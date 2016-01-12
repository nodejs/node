//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

WScript.Echo("Scenario: Multiple closures with multiple variables");

function f()
{
    var x = 12;
    var y = "test";
    var z = 1.1;

    var ret1 = function()
    {
        WScript.Echo("1st function");
        WScript.Echo(x);
        WScript.Echo(y);
        WScript.Echo(z);
    }

    var ret2 = function()
    {
        WScript.Echo("2nd function");
        WScript.Echo(x);
        WScript.Echo(y);
        WScript.Echo(z);
    }

    return [ret1,ret2];
}

function g(funcs)
{
    funcs[1]();
    funcs[0]();
}

var clo = f();
g(clo);
