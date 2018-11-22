// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --expose-gc --verify-heap

C = class {};
for (var i = 0; i < 5; ++i) {
  gc();
  if (i == 2) %OptimizeOsr();
  C.prototype.foo = i + 9000000000000000;
}
