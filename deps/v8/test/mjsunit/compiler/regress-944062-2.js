// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function includes(key, array) {
  // Transition to dictionary mode in the final invocation.
  array.__defineSetter__(key, () => {});
  // Will then read OOB.
  return array.includes(1234);
}
includes("", []);
includes("", []);
%OptimizeFunctionOnNextCall(includes);
includes("", []);
includes("1235", []);
