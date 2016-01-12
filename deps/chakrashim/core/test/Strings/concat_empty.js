//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

WScript.Echo("Concatenation of empty strings.");

var x = "";
var y = "";

x += y;
x = x + y;
x += "";
y += "";
x = x = "";

for(var i = 0; i < 10000; ++i)
{
    x += x + y + x + y + x + "";
}

WScript.Echo("{" + x + "}");
