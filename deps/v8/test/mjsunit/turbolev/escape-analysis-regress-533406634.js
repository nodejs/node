// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev --turbolev-escape-analysis

const o = { abc : 0x80000000 };
o.abc = 127; // Making AnyTagged repr.

function foo() {
  for (let i = 0; i < 5; i++) {

    function bar(arg) {
      try {
        (arg ?? o).abc;
      } catch (e) {}

      const ret = {};
      ret.weeks = ret;
      return ret;
    }

    bar(bar());
  }
}

%PrepareFunctionForOptimization(foo);
foo();
foo();

%OptimizeFunctionOnNextCall(foo);
foo();
