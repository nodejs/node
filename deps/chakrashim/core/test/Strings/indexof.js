//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

var str = "abbbagfedcabbba";

WScript.Echo(str.indexOf("abb"));
WScript.Echo(str.indexOf("abb", 1));
WScript.Echo(str.indexOf("abb", 2));
WScript.Echo(str.indexOf("bba", 3));
WScript.Echo(str.indexOf("bba", 4));
WScript.Echo(str.indexOf("xyz"));
WScript.Echo(str.indexOf("bgf"));
WScript.Echo(str.indexOf("acde"));
WScript.Echo(str.indexOf("edca"));
WScript.Echo(str.indexOf(""));
WScript.Echo(str.indexOf("", 11));

var str2 = "\0\0dcba\0";
WScript.Echo(str2.indexOf("\0\0"));
WScript.Echo(str2.indexOf("\0dc"));
WScript.Echo(str2.indexOf("ba\0"));

var str3 = "abb";
WScript.Echo(str3.indexOf("abbbagfedcabbba"));

var str4 = "\u0100\u0111\u0112\u0113";
WScript.Echo(str4.indexOf("\u0112\u0113"));

//implicit calls
var a = 1;
var b = 2;
var obj = {toString: function(){ a=3; return "Hello World";}};
a = b;
Object.prototype.indexOf = String.prototype.indexOf;
var f = obj.indexOf("e");
WScript.Echo (a);