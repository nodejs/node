//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

// Make sure we invalidate missing property caches, if the property is shadowed on the proto chain.  Also verify it all works with object type specialization.
var SimpleObject = function () {
    this.a = 1;
    this.b = 2;
}

var p = {};
SimpleObject.prototype = p;

var o = new SimpleObject();

function test() {
    var a = o.a;
    var b = o.b;
    var m = o.m;
    WScript.Echo("o.m = " + m);
}

// Run once, walk the proto chain on the slow path not finding property v anywhere, cache it.
test();
// Time to do simple JIT
// From JIT-ed code retrieve the value of v (undefined) from the missing property cache.
test();
// Time to do full JIT (including object type specialization).
// From JIT-ed code retrieve the value of v (undefined) from the missing property cache.
test();
// Now add the property to the prototype, which should invalidate the cache, and force bailout from JIT-ed code.
p.m = 0;
// Verify we get the new value now.
test();

