//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

var a = new Object();
a.toString = function() { WScript.Echo("In toString() ");  return "foo" }
var v = String.prototype.toLowerCase.call(a);
WScript.Echo("Test call ToString - user defined object: " + v);

a = true;
v = String.prototype.toLowerCase.call(a);
WScript.Echo("Test call ToString - bool: " + v);

a = 123
v = String.prototype.toLowerCase.call(a);
WScript.Echo("Test call ToString - number: " + v);

a = new Date();
a.setTime(20000)
v = String.prototype.toLowerCase.call(a);
WScript.Echo("Test call ToString - date: " + v);