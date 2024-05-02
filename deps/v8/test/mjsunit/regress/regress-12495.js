// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function opt() {
  try {
    Reflect.apply("".localeCompare, undefined, [undefined]);
    return false;
  } catch(e) {
    return true;
  }
}

%PrepareFunctionForOptimization(opt);
assertTrue(opt());
assertTrue(opt());
%OptimizeFunctionOnNextCall(opt);
assertTrue(opt());
assertTrue(opt());
