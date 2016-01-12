//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

// - 'o2' is hoisted outside the loop
// - 'test0b' is inlined
// - 'o2 = 0' should kill 'o2' in the inliner function 'test0', causing the loop to exit early
function test0() {
    var o1 = { p: 0 };
    var o2 = { p: 4 };
    for(; o1.p < o2.p; ++o1.p)
        test0b();
    return o1.p;

    function test0a() { eval(""); }
    function test0b() { o2 = 0; }
}
WScript.Echo("test0: " + test0());
