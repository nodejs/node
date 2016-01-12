//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------


var arr;
function test(param)
{
    arr = [ 1, function() { return param; }, 2];
}

test("test1");
WScript.Echo(arr[1]());
test("test2");
WScript.Echo(arr[1]());
