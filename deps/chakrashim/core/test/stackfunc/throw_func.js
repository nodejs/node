//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------


function test(param)
{
    throw function() { return param; }

}

function runtest(param)
{

    try
    {
        test(param)
    }
    catch (e)
    {
        WScript.Echo (e());
    }
}

runtest("test1");
runtest("test2");

