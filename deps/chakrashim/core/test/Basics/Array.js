//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

var a = new Array(10);
var b = new Array();
var c = new Array("muck", 3.2, 0, 18);
c[-1] = "minus 1";

WScript.Echo("Store a single item in a");
a[1] = 10;
WScript.Echo(a[1]);

WScript.Echo("Store a single item in b");
b[3] = 99;
WScript.Echo(b[3]);

for (var i = -1; i < c.length; i++) {
    WScript.Echo(c[i]);
}

// Test boolean expressions in an initializer
var x = {}, y = false;
WScript.Echo([x||y]);
WScript.Echo([x&&y]);
WScript.Echo([x ? y : x]);
WScript.Echo([y ? x : y]);
WScript.Echo([y||x]);
WScript.Echo(y&&x);
WScript.Echo([x||y, x&&y, x ? y : x, y ? x : y, y||x, y&&x]);

// Test some boundary property names
var o = [];

o["4294967294"] = 100;
WScript.Echo(o["4294967294"]);

o["4294967295"] = 101;
WScript.Echo(o["4294967295"]);

o["4294967296"] = 102;
WScript.Echo(o["4294967296"]);

o["4088701331"] = 103;
WScript.Echo(o["4088701331"]);

o["40887013312"] = 104;
WScript.Echo(o["40887013312"]);

o["4088.7013"] = 105;
WScript.Echo(o["4088.7013"]);

o["408870133X"] = 106;
WScript.Echo(o["408870133X"]);
