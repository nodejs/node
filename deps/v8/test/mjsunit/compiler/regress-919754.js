// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax


function f(get, ...a) {
  for (let i = 0; i < 1000; i++) {
    if (i === 999) %OptimizeOsr();
    a.map(f);
  }
  return get();
}
%PrepareFunctionForOptimization(f);
assertThrows(f);
