// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev

function optimizeOnNextCall(f) {
  %OptimizeMaglevOnNextCall(f);
}
const assertTierOptimized = assertMaglevved;

d8.file.execute('test/mjsunit/maglev/test-in-helpers.js');
