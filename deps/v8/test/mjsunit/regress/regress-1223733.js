// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turboprop --turbo-dynamic-map-checks

function main() {
  // Store something onto a function prototype so we will bailout of the
  // function.prototype load optimization in NativeContextSpecialization.
  isNaN.prototype = 14;
  const v14 = isNaN.prototype;
}
%PrepareFunctionForOptimization(main);
main();
%OptimizeFunctionOnNextCall(main);
main();
