//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

var a = [true, false, new Boolean(true), new Boolean(false)];
var b = [true, false, new Boolean(true), new Boolean(false), -1, 0, 1, 2, 1.0, 1.1, 0.0, +0, -0, null, undefined, new Object(), "", "abc", "-1", "0", "1", "2", "true", "false", "t", "f", "True", "False", " 1.00 ", " 1. ", " +1.0 ", new Number(0), new Number(1)];

for (var i = 0; i < a.length; i++)
{
    for (var j = 0; j < b.length; j++)
    {
        WScript.Echo(a[i] + " == " + b[j] + ": " + (a[i] == b[j]));
    }
}
