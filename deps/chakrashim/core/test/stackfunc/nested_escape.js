//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function test(param)
{
    function nested()
    {
        var nested_escape = function()
        {
            return param;
        }

        var inner = function()
        {
            return nested_escape;
        }
        return inner();
    }
    return nested();
}

WScript.Echo((test("test1"))());
WScript.Echo((test("test2"))());
