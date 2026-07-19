// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax

class C1 extends Object {}
class C2 extends C1 {
  constructor() { super(); }
}
const v4 = %PrepareFunctionForOptimization(C2);
Reflect.construct(C2, [v4,v4,v4,v4,v4], Proxy);
const v9 = %OptimizeFunctionOnNextCall(C2);
Reflect.construct(C2, [C1,C1,C1,C1,C1]);
