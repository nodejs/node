// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --cache=after-execute --allow-natives-syntax --lazy-compile-dispatcher

function target(x) {
  if (x) return new Error().stack;
  return 2;
}
const kIsLazy = 1 << 17;
if (!(%GetOptimizationStatus(target) & kIsLazy)) {
  target(true);
} else {
  %DebugToggleBlockCoverage(true);
  target(false);
  let toggled = false;
  function recurse() {
    try {
      recurse();
    } catch (e) {
      if (!toggled) {
        toggled = true;
        %DebugToggleBlockCoverage(false);
      }
    }
  }
  recurse();
}
