//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

var a = 0;
var Failed = 0;
function foo(num)
{
    if (num == null) 
    Failed++;
    if (num*1.1 == null) 
    Failed++;
}

a = 0.1;
for (var i = 0; i < 1000; i++)
    foo(100 * a);

a = 0;
foo(100 * a);

if (Failed)
    WScript.Echo("FAILED\n");
else
    WScript.Echo("Passed\n");
