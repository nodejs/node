//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

var base = new Array(100);
base[5] = 50;

var A = new Array(100);
A[10] = 100;
Array.prototype[5] = 50;

function foo(arr, i)
{
    i = arr[i];
    if (i != 50)
    WScript.Echo("FAILED");
    return i;
}

for (var i = 0; i < 1000; i++)
    foo(A, 5);

if (foo(A, 5) == 50) WScript.Echo("Passed");
