// Copyright 2024 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev --turbofan

function dematerialized_object_template(n) {
  let o = { x : 42 };
  if (n < 0) {
    // We won't execute this code when collecting feedback, which means that
    // Maglev will do an unconditional deopt here.
    n + 5;
    return o;
  }
  o.x = 17;
  if (n == 1) {
    // We won't execute this code when collecting feedback, which means that
    // Maglev will do an unconditional deopt here.
    n + 5;
    return o;
  }
  return n + 2;
}

let dematerialized_object =
    copyFunctionWithFreshFeedback(dematerialized_object_template);
%PrepareFunctionForOptimization(dematerialized_object);
assertEquals(7, dematerialized_object(5));
assertEquals(7, dematerialized_object(5));

%OptimizeFunctionOnNextCall(dematerialized_object);
assertEquals(7, dematerialized_object(5));

assertEquals({x : 42}, dematerialized_object(-1));

dematerialized_object =
    copyFunctionWithFreshFeedback(dematerialized_object_template);
%PrepareFunctionForOptimization(dematerialized_object);
assertEquals(7, dematerialized_object(5));
assertEquals(7, dematerialized_object(5));

%OptimizeFunctionOnNextCall(dematerialized_object);
assertEquals(7, dematerialized_object(5));

assertEquals({x : 17}, dematerialized_object(1));
