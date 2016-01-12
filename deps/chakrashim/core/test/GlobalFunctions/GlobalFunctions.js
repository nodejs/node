//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

var a = eval("1+1");
eval("a");
eval.foo = "Expando_In_Eval_Ok";
WScript.Echo(eval.foo);

var nn = isNaN(Number.NaN);
WScript.Echo("test: isNaN(Number.NaN) : " + nn);

nn = isNaN(123);
WScript.Echo("test: isNaN(123) : " + nn);
isNaN.foo = "Expando_In_IsNaN_Ok";
WScript.Echo(isNaN.foo);

nn = isFinite(Number.POSITIVE_INFINITY);
WScript.Echo("test: isFinite(Number.POSITIVE_INFINITY) : " + nn);

nn = isFinite(123);
WScript.Echo("test: isFinite(123) : " + nn);
isFinite.foo = "Expando_In_IsFinite_Ok";
WScript.Echo(isNaN.foo);


WScript.Echo("*** Test URI  functions ***");
var checkChar = "\u00a9";
WScript.Echo("Test print wchar: \"\\u00a9\" ");
WScript.Echo(checkChar);

var a = encodeURI("\u00a9");
WScript.Echo("Test encode : encodeURI(\"\\u00a9\");");
var a = encodeURI("\u00a9");
WScript.Echo(a);

WScript.Echo("Test decode back: ")
var b = decodeURI(encodeURI("\u00a9"));
WScript.Echo(b);

WScript.Echo("Test encode :  encodeURI(\"http:\/\/www.isp.com\/app.cgi?arg1=1&arg2=hello world\");");
a = encodeURI("http://www.isp.com/app.cgi?arg1=1&arg2=hello world");
WScript.Echo(a);
WScript.Echo("Test decode back: ")
b = decodeURI(encodeURI("http://www.isp.com/app.cgi?arg1=1&arg2=hello world"));
WScript.Echo(b);


WScript.Echo("Test encode component : encodeURIComponent(\"http\");");
a = encodeURIComponent("http");
WScript.Echo(a);
WScript.Echo("Test decode component back: ")
b = decodeURIComponent(encodeURIComponent("http"));
WScript.Echo(b);

WScript.Echo("Test encode component : encodeURIComponent(\"\/\/www.isp.com\/app.cgi\");");
a = encodeURIComponent("//www.isp.com/app.cgi");
WScript.Echo(a);
WScript.Echo("Test decode component back: ")
b = decodeURIComponent(encodeURIComponent("//www.isp.com/app.cgi"));
WScript.Echo(b);

WScript.Echo("Test encode component : encodeURIComponent(\"arg1=1&arg2=hello world\");");
a = encodeURIComponent("arg1=1&arg2=hello world");
WScript.Echo(a);
WScript.Echo("Test decode component back: ")
b = decodeURIComponent(encodeURIComponent("arg1=1&arg2=hello world"));
WScript.Echo(b);

WScript.Echo("Test global constants: ");
WScript.Echo(Infinity);
WScript.Echo(undefined);

WScript.Echo("Escape Unescape ");
WScript.Echo(escape("Hello World"));
WScript.Echo(unescape("Hello%20World"));
WScript.Echo(unescape(escape("foo bar")));
WScript.Echo(unescape("It%27s%20a%20test%21"));