// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

// Provoke type None as result of a SpeculativeNumberMultiply to
// ensure that Turbofan can handle this.

function inlined(b, x) {
  if (b) {
    x * 2 * 2
  }
}

inlined(true, 1);
inlined(true, 2);
inlined(false, 1);

function foo(b) { inlined(b, "") }
foo(false); foo(false);
%OptimizeFunctionOnNextCall(foo);
foo(true);
