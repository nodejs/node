//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

WScript.Echo(/(a|ab)/.exec("ab"));
WScript.Echo(/(ab|a)/.exec("ab"));

r = /(aaab|aaa)/;
a = "aaabab";

WScript.Echo(a.match(r));

r = /(aaa|aaab)/;
a = "aaabab";

WScript.Echo(a.match(r));

r = /(a|ab)*/;
a = "abab";

WScript.Echo(a.match(r));

r = /((a|ab)*)?/;
a = "abab";

WScript.Echo(a.match(r));

r = /(a|ab)?/;
a = "abab";

WScript.Echo(a.match(r));

r = /(p\/.*)?(.*)/;
a = "p/u";
var result = r.exec(a);
WScript.Echo(result+"");

var x = new RegExp("(([^:]*)://([^:/?]*)(:([0-9]+))?)?([^?#]*)([?]([^#]*))?(#(.*))?");
var y = "http://shop.ebay.com/i.html?rt=nc&LH_FS=1&_nkw=dell+optiplex&_fln=1&_trksid=p3286.c0.m283";
WScript.Echo(y.match(x));
