// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --no-lazy-feedback-allocation --maglev-non-eager-inlining
// Flags: --max-maglev-inlined-bytecode-size-small=0

function load(a, i) {
  return a[i];
}

var a = new Int32Array(1000);
for (var i = 0; i < 1000; ++i) {
  var x = 0;
  for (var j = 0; j < a.length; ++j) {
    x += load(a, j);
  }
}
