// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Exception handlers: the "↳ throw" deopt-frame form.
//
// Getting Maglev to emit one is fussier than it looks. The call inside the try
// must survive as a real throwing node with an attached handler, which needs:
//
//   1. the callee NOT inlined  -- otherwise Maglev sees the body and the call
//      node disappears. Kept over the inlining size limit on purpose, the same
//      trick inlining.js uses.
//   2. the throw path actually taken -- otherwise the handler is unreachable
//      and gets swept. Hence the (x & 1023) === 0 trip every 1024 iterations.
//
// Two catch shapes, because the printer has two forms:
//   risky() keeps `acc` live across the catch  -> handler block has a phi
//                                                 -> "↳ throw @26 (b2) : {…}"
//   bare()  keeps nothing live                 -> no phi
//                                                 -> "↳ throw (b2)"

function mayThrow(x) {
  if ((x & 1023) === 0) throw new Error("boom");
  let t = x;
  t = t + 1; t = t ^ 3; t = t + 2; t = t ^ 5; t = t + 4; t = t ^ 7;
  t = t + 8; t = t ^ 11; t = t + 16; t = t ^ 13; t = t + 32; t = t ^ 17;
  t = t + 64; t = t ^ 19; t = t + 128; t = t ^ 23; t = t + 256; t = t ^ 29;
  return t;
}

// Live value across the handler -> catch block has a phi.
function risky(k) {
  let acc = k * 2;
  try { acc += mayThrow(k); } catch (e) { acc = acc - 1; }
  return acc;
}

// Nothing live across the handler -> catch block has no phi.
function bare(k) {
  try { mayThrow(k); } catch (e) {}
  return 0;
}

let sink = 0;
for (let i = 1; i < 400000; i++) {
  sink += risky(i);
  sink += bare(i);
}
