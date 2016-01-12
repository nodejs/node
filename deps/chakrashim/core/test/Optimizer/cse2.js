//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function foo(arg1, arg2, arg3, arg4) {
    var y = 1;
    if (arg3) {
        Object.defineProperty(arg1, arg4, getProp);
    }
    y = arg1[arg4];
    if (arg3)
        y = arg1[arg4];
    return y;
}

o1 = { "prop4": 4 };
var count = 0;

for (var i = 0; i < 200; i++) {
    foo(o1, "text", false, "prop4");
}

var getProp = { get: function () { return count++; } };

if (foo(o1, "text", true, "prop4") != 1)
    WScript.Echo("FAILED");
else
    WScript.Echo("Passed");


