// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

class C0 {
}
class C1 extends C0 {
}

%PrepareFunctionForOptimization(C1);
const v10 = {
  get(a5, a6) {
      Object.setPrototypeOf(a5, {});
      return Reflect.get(a5, a6);
  },
};
const v11 = new Proxy(C0, v10);
%OptimizeMaglevOnNextCall(C1);
Reflect.construct(C1, [C1,C1], v11);
