// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev-assert --turbolev

async function foo() {
  for (let i = 0; i < 5; i++) {
    try {
      Realm.navigate(i);
    } catch (e) {
      [i];
    }
    while (i > 4) {
      await 42;
    }
  }
}

%PrepareFunctionForOptimization(foo);
foo();
%OptimizeFunctionOnNextCall(foo);
foo();
