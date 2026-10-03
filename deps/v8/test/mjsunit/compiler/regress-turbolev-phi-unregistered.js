// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --turbolev --allow-natives-syntax --no-use-ic --trace-turbo

function* testGenerator(root) {
  let keys = Object.getOwnPropertyNames(root);
  for (let key of keys) {
    yield root[key];
  }
}

%PrepareFunctionForOptimization(testGenerator);
for (let x of testGenerator(this)) {}
%OptimizeFunctionOnNextCall(testGenerator);
for (let x of testGenerator(this)) {}
