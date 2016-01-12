//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

WScript.Echo("Test 1: Math.E");
var obj = Object.create(Math);
obj.E = "foo";
WScript.Echo(obj.E);

WScript.Echo("Test 2: function length");
function myFunc(a, b, c, d) {}
obj = Object.create(myFunc);
obj.length = "foo";
WScript.Echo(obj.length);

WScript.Echo("Test 3: Regular expression properties");
var regExp = new RegExp("/abc/g");
obj = Object.create(regExp);
obj.global = "foo";
WScript.Echo(obj.global);

obj.lastIndex = "foo";
WScript.Echo(obj.lastIndex);

WScript.Echo("Test 4: String length");
obj = Object.create(new String("test"));
obj.length = "foo";
WScript.Echo(obj.length);
