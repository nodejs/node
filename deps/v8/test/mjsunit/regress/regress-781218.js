// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

var m = new Map();

function C() { }

// Make sure slack tracking kicks in and shrinks the default size to prevent
// any further in-object properties.
%CompleteInobjectSlackTracking(new C());

function f(o) {
  o.x = true;
}

// Warm up {f}.
f(new C());
f(new C());


var o = new C();
%HeapObjectVerify(o);

m.set(o, 1);  // This creates hash code on o.

// Add an out-of-object property.
o.x = true;
%HeapObjectVerify(o);
// Delete the property (so we have no out-of-object properties).
delete o.x;
%HeapObjectVerify(o);


// Ensure that growing the properties backing store in optimized code preserves
// the hash.
%OptimizeFunctionOnNextCall(f);
f(o);

%HeapObjectVerify(o);
assertEquals(1, m.get(o));
