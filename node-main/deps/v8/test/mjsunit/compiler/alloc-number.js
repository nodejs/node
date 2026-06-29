// Copyright 2010 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --max-semi-space-size=1 --max-old-space-size=10

// This test is specific to release builds. alloc-number-debug.js
// has a fast version for debug builds.

// Try to get a GC because of a heap number allocation while we
// have live values (o) in a register.
function f(o) {
  var x = 1.5;
  var y = 2.5;
  for (var i = 1; i < 10000; i+=2) o.val = x + y + i;
  return o;
}

var o = { val: 0 };
for (var i = 0; i < 10; i++) f(o);
