//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function f(o)
{
    var s;
    for (var i = 0; i < 2; i++)
    {
        s = typeof o.x;
        var f = o.x;
    }
    return s;
}

WScript.Echo(f({x:1}));
