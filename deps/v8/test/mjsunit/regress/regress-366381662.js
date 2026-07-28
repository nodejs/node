// Copyright 2024 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax --maglev-object-tracking

function opt_me(a) {
  if (!a > 0) {
      a++;
      return 1;

      // Force context allocation of a, so that loads of a are loads from the
      // context.
      (() => {
          a;
      })();
  }

  return a;
}

%PrepareFunctionForOptimization(opt_me);
opt_me();
%OptimizeMaglevOnNextCall(opt_me);
opt_me();

let o = {};
assertEquals(o, opt_me(o));
