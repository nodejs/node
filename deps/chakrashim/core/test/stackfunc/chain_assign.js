//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------


function test(param)
{
    var x;
    var y;
    x = (y = function() { return param; });

    return x;
}

WScript.Echo((test("test1"))());
WScript.Echo((test("test2"))());
