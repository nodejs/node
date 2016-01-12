//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function test0() {
    var loopInvariant = 5;
    function makeArrayLength() {
    }
    var func0 = function() {
    };
    obj1 = func0;
    var __loopvar1 = loopInvariant;
    do {
        __loopvar1++;
    } while(__loopvar1 < loopInvariant);
}
test0();
test0();

WScript.Echo("pass");
