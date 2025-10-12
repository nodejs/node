// Copyright 2024 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

class C1 extends String {
  constructor() {
    super();
    assertEquals(0, super.length);
  }
}

%PrepareFunctionForOptimization(C1);
new C1();

%OptimizeFunctionOnNextCall(C1);
new C1();
