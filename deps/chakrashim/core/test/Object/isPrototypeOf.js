//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function write(v) { WScript.Echo(v + ""); }

var o = new Object();
var a = [11,12,13];
var d = new Date();

write("------------ isPrototypeOf ------------");
write(Object.prototype.isPrototypeOf(o));
write(Object.prototype.isPrototypeOf(a));

write(Array.prototype.isPrototypeOf(o));
write(Array.prototype.isPrototypeOf(a));

// v2/IE8 compatibility
write(d.isPrototypeOf(d));
