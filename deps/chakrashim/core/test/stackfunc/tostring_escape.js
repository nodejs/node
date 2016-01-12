//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------


var escape;

Function.prototype.toString = function()
{
    escape = this;
    return "toString";
}

function test3(param)
{
    var func3 = function() { return param; }
    return func3 + "3";
}

WScript.Echo(test3("test1"));
WScript.Echo(escape());
WScript.Echo(test3("test2"));
WScript.Echo(escape());

