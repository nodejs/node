//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

WScript.Echo("Scenario: Closure with multiple variables");

function f(a)
{
    var x = 12;
    var y = "test";
    var z = 1.1;

        var ret = function()
        {
                WScript.Echo(a);
                WScript.Echo(x);
                WScript.Echo(y);
                WScript.Echo(z);
        }

    return ret;
}

function g(func)
{
    func();
}

var clo = f("ArgIn");
g(clo);
