//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function test(a, b)
{
    return a/b;
}

for (var i = 0; i < 100; i++)
{   
    test(i<<1, 2);
}

var x = 8;
if (test((0x80000000>>8)<<x, -1) === 2147483648)
{
    WScript.Echo("Passed");
}
else
{
    WScript.Echo("FAILED");
}
