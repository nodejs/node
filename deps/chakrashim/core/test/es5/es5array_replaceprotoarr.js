//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------


function test(o)
{
    o[0] = 1;
}
var arr = [];
function Ctor() { this[1] = 2; };
Object.defineProperty(arr, "0", { value: 0, writable: false });
var o = new Ctor();
o.__proto__ = arr;
test(o);
WScript.Echo(o[0]);
var o = new Ctor();
o.__proto__ = arr;
test(o);
WScript.Echo(o[0]);
