//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------


function t()
{
    throw "PASS";
}
function f()
{
    try 
    {
        while (true) { t(); } 
    }
    catch (e) 
    {
        WScript.Echo(e);
    }
}
f();

