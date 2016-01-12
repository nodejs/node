//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

var Failed = false;

function FAIL() { Failed = true; }

function test() {
    var x = x instanceof x;
    function x() { };
    if (x !== false)
    FAIL();
};

test();
test();

if (!Failed)
    WScript.Echo("Passed");
