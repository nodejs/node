// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --enable-slow-asserts --debug-code

function foo(p) {
  for (var i = 0; i < 100000; ++i) {
    p = Math.min(-1, 0);
  }
}
foo(0);
