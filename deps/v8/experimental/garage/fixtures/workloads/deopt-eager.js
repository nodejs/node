// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Eager deopt via a map change: `read` is optimized for {x} objects, then fed an
// object with a different map. Exercises --trace-deopt / --trace-deopt-verbose
// and the deopt -> compilation correlation keys (docs/correlation-keys.md).
function read(o) {
  return o.x + 1;
}

let sink = 0;
for (let i = 0; i < 100000; i++) {
  sink += read({x: i});
}

// Different map -> wrong-map deopt in the optimized code.
sink += read({y: 1, x: 2});

// Re-warm so the function optimizes a second time (two compilation instances
// for the same function, which the sidebar must keep distinct).
for (let i = 0; i < 100000; i++) {
  sink += read({y: 1, x: i});
}
print(sink);
