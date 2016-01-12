//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------


function test(str)
{
    try
    {
        eval(str);
    }
    catch (e)
    {
        WScript.Echo(e);
    }
}

test("var a = { 1} ");
test("var a = { 0.01 } ");
test("var a = { \"s\" } ");
