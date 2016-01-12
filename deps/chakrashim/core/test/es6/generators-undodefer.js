//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function f() { }
var o = { };
o.gf = function* () { yield 0; };

if (o.gf().next().value === 0) {
    WScript.Echo("passed");
} else {
    WScript.Echo("failed");
}
