// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --cache=after-execute --strict-termination-checks

d8.terminate();

function f(a, b) {
  return a + b
}

// Force an OSR to handle interrupts.
for (let i = 0; i < 1_000; i++) f(i, i)
