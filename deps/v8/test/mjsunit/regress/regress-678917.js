// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --max-old-space-size=1600 --nostress-incremental-marking

// This test uses a lot of memory and fails with flaky OOM when run
// with --stress-incremental-marking on TSAN.

s1 = 'xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx';
s1 += s1;
s1 += s1;
s1 += s1;
s1 += s1;

s0 = 'a';

function g() {
  for (var j = 0; j < 1000000; j++) {
    s0 += s1;
  }
}

try {
  g();
} catch (e) {
}

assertEquals('x', s0[10]);
