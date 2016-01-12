//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

// -version:2 -forcedeferparse
function test0(x) {
    with (x)
    {
        z = "4";
    };

    return x;
};

var p={o:1, z:2};
WScript.Echo("p = " + JSON.stringify(p));
var k=test0(p);
WScript.Echo("k = " + JSON.stringify(k));
WScript.Echo("k.z = " + k.z);
WScript.Echo("k.z+1 = " + k.z+1);
