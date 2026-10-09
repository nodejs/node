// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --cache=code --code-cache-source-hash-sha256

function foo(a, b) {
  return a + b;
}

assertEquals(foo(10, 20), 30);
