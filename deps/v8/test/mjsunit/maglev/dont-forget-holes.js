// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax --maglev

function foo(a) {
  return a[1];
}

%PrepareFunctionForOptimization(foo);

// Read a hole from a holey double array.
foo([0.5, , 1.5]);

// No hole.
foo([0.5, 1.0, 1.5]);

%OptimizeMaglevOnNextCall(foo);

// Read a hole again.
foo([0.5, , 1.5]);

// No deopts since we generated code to handle the hole.
assertTrue(isMaglevved(foo));
