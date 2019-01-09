// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function* E(b) {
  while (true) {
    for (yield* 0; b; yield* 0) {}
  }
}

%OptimizeFunctionOnNextCall(E);
E();
