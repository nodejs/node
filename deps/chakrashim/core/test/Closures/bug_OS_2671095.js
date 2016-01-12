//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function test() {
    function inner() {
        (function f() { eval(""); })();
    }
    inner();
}
test();
WScript.Echo("passed");
