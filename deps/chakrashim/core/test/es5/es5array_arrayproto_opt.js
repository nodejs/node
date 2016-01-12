//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------


function test(obj, i)
{
    obj[i] = 1;
    WScript.Echo(obj[i]);
}

function test2(obj)
{
    obj[0] = 1;
    WScript.Echo(obj[0]);
}


// Populate the profile with any array that we are growing a segment length
test2([]);
Object.defineProperty(Array.prototype, "0", { value:"blah", writable: false });

// Test the jitted code with the array type check elimination
test2([]);

var arr = [];
arr[1] = 2;
// Populate the profile to not filling a missing value.
test(arr, 1);
// Test the jitted code with the array type check elimitation
test(arr, 0);

