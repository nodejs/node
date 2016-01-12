//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

// Most basic test of missing property caching.
function test() {
    var o = {};
    var v = o.a;
    WScript.Echo("v = " + v);
}

// Run once, walk the proto chain on the slow path not finding property v anywhere, cache it.
test();
// Retrieve the value of v (undefined) from the missing property cache.
test();

