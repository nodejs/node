//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

var d = ArrayBuffer.prototype;
WScript.Echo(d);
d.aaa = 20;
var a = Object.getPrototypeOf(new Int8Array(1).buffer);
a.foo = 20;
a.bar = 42;
WScript.Echo(a);
WScript.Echo(a.foo);
var b = Object.getPrototypeOf(new Int16Array(0).buffer);
WScript.Echo(b);
for (var i in b)
{
WScript.Echo(i + ' = ' + b[i]);
}
WScript.Echo(b.foo);
var c = Object.getOwnPropertyNames(b);
for (var i in c) 
{
WScript.Echo(c[i]);
}

WScript.Echo(a == b);

var e = new Int32Array(0).buffer.constructor.prototype;
WScript.Echo(e.foo);
for (var i in e)
{
WScript.Echo(i + ' = ' + e[i]);
}
var ee = Object.getOwnPropertyNames(e);
for (var i in ee) 
{
WScript.Echo(ee[i]);
}
