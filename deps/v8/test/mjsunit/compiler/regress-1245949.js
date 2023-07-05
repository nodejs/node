// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function main() {
  const v2 = Date.now();
  const v3 = /z/;
  v3.test(v2);
}

%PrepareFunctionForOptimization(main);
main();
%OptimizeFunctionOnNextCall(main);
main();
