//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------


function test(param)
{
    var nested = function() { return param; }
    try
    {
        var inner = function() { return nested(); }
        WScript.Echo(inner());
    }
    catch (e)
    {
    }

    WScript.Echo(inner());
}


test("test1");
test("test2");
