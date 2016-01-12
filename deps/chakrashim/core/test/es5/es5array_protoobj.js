//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------


function test(o)
{
    o[0] = 1;
}
var obj = {};
function Ctor() { this[1] = 2; };
Ctor.prototype = obj;
Object.defineProperty(obj, "0", { value: 0, writable: false });
var o = new Ctor();
test(o);
WScript.Echo(o[0]);
var o = new Ctor();
test(o);
WScript.Echo(o[0]);
