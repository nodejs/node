// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function foo(arg) {
  var value;
  // None of the branches of this switch are ever taken, but
  // the sequence means value could be the hole.
  switch (arg) {
    case 1:
      let let_var = 1;
    case 2:
      value = let_var;
  }
  // Speculative number binop with NumberOrOddball feedback.
  // Shouldn't be optimized to pure operator since value's phi
  // could theoretically be the hole (we would have already thrown a
  // reference error in case 2 above if so, but TF typing still
  // thinks it could be the hole).
  return value * undefined;
}

foo(3);
foo(3);
%OptimizeFunctionOnNextCall(foo);
foo(3);
