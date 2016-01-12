//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------


var obj;
function test(param)
{
    obj = { x: 1, y: function() { return param; } };
}

test("test1");
WScript.Echo(obj.y());
test("test2");
WScript.Echo(obj.y());
