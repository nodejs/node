// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --throws

function r() {
  return new Promise((resolve, reject) => {
    throw new Error("boom");
  });
}

function main() {
  [].values().__proto__.return = r;
  try {
    new WeakSet([1]);
  } catch (err) {}
}

%PrepareFunctionForOptimization(r);
main();
%OptimizeFunctionOnNextCall(r);
main();
