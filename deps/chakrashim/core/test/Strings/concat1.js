//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

WScript.Echo("Basic string concatenation.");

var n = 5;
var x = new Array(n);

var count = 1;
for(var i = 0; i < n; ++i)
{
    var c = String.fromCharCode(97+i);
    x[i] = "";

    for(var j = 0; j < count; ++j)
    {
        x[i] += c;
    }

    count *= 3;
}

var str = x[0];
str += x[1] + x[2];
str += "XXXX";
str += x[3] + "XXXX";
str += str + x[4] + str + x[4];
str += str + x[0] + str;
str += "XXXX";

WScript.Echo(str);
