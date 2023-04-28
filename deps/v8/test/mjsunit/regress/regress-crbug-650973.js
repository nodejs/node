// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

var v = {p:0};
// Turn the object into dictionary mode.
v.__defineGetter__("p", function() { return 13; });

function f() {
  var boom = (v.foo = v);
  assertEquals(v, boom.foo);
}

f();
f();
f();
