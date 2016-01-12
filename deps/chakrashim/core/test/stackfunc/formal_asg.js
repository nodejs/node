//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------



function test(param)
{
    var l = param;
    param = function() { return l; }

    return arguments[0];
}


WScript.Echo(test("test1")());
WScript.Echo(test("test2")());

