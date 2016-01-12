//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function testStrict() {
    "use strict";
    try {
        var o3 = new String("aaa");
        o3[0] = "b";
        WScript.Echo("failed");
    } 
    catch (e) {
        WScript.Echo("passed");
    }
}
testStrict();

function testNonStrict() {
    try {
        var o3 = new String("aaa");
        o3[0] = "b";
        if (o3 != "aaa")
            WScript.Echo("failed");
        else
            WScript.Echo("passed");
    } 
    catch (e) {
        WScript.Echo("failed");
    }
}
testNonStrict();