//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

var Failed = false;

function foo(last)
{
    var x = 0.1;
    var y = 0.2;
    var z = "Louie ";
    var w;

    for (var i = 0; i < 3; i++)
    {
        w = x + x;  // Type spec this add? Make sure there is a bailout...
        x = y;
        y = z;
    }

    if (w != "Louie Louie ")
    {
        WScript.Echo("w = " + w);
        WScript.Echo("FAILED");
        Failed = true;
    }
    else if (last && !Failed)
    {
        WScript.Echo("Passed");
    }
}

for (var i = 0; i < 500; i++)
{
    foo(false);
}

foo(true);
