// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax

class C extends (class {}) {
  constructor() {
    var f = () => {
      try { C.__proto__ = null; } catch {}
      try { super(); } catch {}
    };
    %PrepareFunctionForOptimization(f);
    f();
    %OptimizeMaglevOnNextCall(f);
  }
}
try { new C(); } catch {}
// The next 2 calls deopt before reaching relevant bits.
try { new C(); } catch {}
try { new C(); } catch {}
try { new C(); } catch {}
