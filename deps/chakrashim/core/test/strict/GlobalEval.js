//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

"use strict";

function testFromInsideFunction() {
    var a = 2;
    var b = 0;
    var c = eval("eval('var b = 3; a + b;');");
    WScript.Echo(a, b, c);
}
testFromInsideFunction();

var a = 2;
var b = 0;
var c = eval("eval('var b = 3; a + b;');");
WScript.Echo(a, b, c);
