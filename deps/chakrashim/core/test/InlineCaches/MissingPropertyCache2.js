//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

// Make sure we invalidate missing property caches, if the property is shadowed on the instance.
var o = {};
var v;

function test() {
    v = o.a;
    WScript.Echo("v = " + o.a);
}

// Run once, walk the proto chain on the slow path not finding property v anywhere, cache it.
test();
// Retrieve the value of v (undefined) from the missing property cache.
test();
// Now add the property to the object, which should invalidate the cache.
o.a = 0;
// Verify we get the new value now.
test();

