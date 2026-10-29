// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev

function f(p, i) {
  const ha = [1.5, , ];
  // `ha[i]` loads a HoleyFloat64 (the array is holey).
  let r = ha[i];
  try {
    if (p & 1) throw 0;
    // Here `ha[i]` is used in a Float64 context, so it is a plain Float64.
    // A reducer collapses this load and the HoleyFloat64 one above to a single
    // Turboshaft value, even though their Maglev representations differ.
    r = ha[i] - 0.0;
    if (p & 2) throw 0;
    return r;
  } catch (e) {
    // Both the HoleyFloat64 and the Float64 flavours of `r` reach this
    // handler's exception phi through distinct throwing predecessors.
    return r;
  }
}

%PrepareFunctionForOptimization(f);
for (let k = 0; k < 960; k++) {
  try { f(k & 3, k & 1); } catch (e) {}
}
%OptimizeFunctionOnNextCall(f);
for (let k = 0; k < 960; k++) {
  try { f(k & 3, k & 1); } catch (e) {}
}
