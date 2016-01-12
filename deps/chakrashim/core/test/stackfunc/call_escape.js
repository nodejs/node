//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function func(x, f)
{
    WScript.Echo(f());
}
function test(param)
{
    var call_escape = function()
    {
        return param;
    }
    func(1, call_escape);
}

test("test1");
test("test2");

