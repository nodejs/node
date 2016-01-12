//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

var x = 100;

function f()
{
    x--;
    return x;
}

for(var i = 0; i < 10; ++i)
{
    var a = i;

    while(f() > 0 && a > 5)
    {
        WScript.Echo("f: " + x);
        --a;
    }
}
