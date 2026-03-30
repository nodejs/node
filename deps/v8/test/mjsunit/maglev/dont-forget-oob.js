// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax --maglev

function foo(a) {
  return a[1];
}

%PrepareFunctionForOptimization(foo);

// Read OOB.
foo([0.5]);

// No OOB from a different elements kind.
foo([1, 2, 3]);

%OptimizeMaglevOnNextCall(foo);

// Read OOB again.
foo([0.5]);

// No deopts since we generated code to handle the OOB.
assertTrue(isMaglevved(foo));
