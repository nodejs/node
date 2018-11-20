// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --gc-global --noincremental-marking --allow-natives-syntax

function Ctor() {
  this.foo = 1;
}

var o = new Ctor();
var p = new Ctor();


function crash(o, timeout) {
  var s = "4000111222";  // Outside Smi range.
  %SetAllocationTimeout(100000, timeout);
  // This allocates a heap number, causing a GC, triggering lazy deopt.
  var end = s >>> 0;
  s = s.substring(0, end);
  // This creates a map dependency, which gives the GC a reason to trigger
  // a lazy deopt when that map dies.
  o.bar = 2;
}

crash(o, 100000);
crash(o, 100000);
crash(p, 100000);
%OptimizeFunctionOnNextCall(crash);
crash(o, 100000);
o = null;
p = null;
crash({}, 0);
