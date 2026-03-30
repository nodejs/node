// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax

function test() {}
function foo() {
  // --test() will throw a ReferenceError, making the rest of the for-of loop
  // dead. The destructuring assignment is still part of the same expression,
  // and it generates a try-finally for its iteration, so if it isn't fully
  // eliminated the finally block may resurrect the destructuring assignment
  // loop.
  for (let [x] of --test()) {}
}

%PrepareFunctionForOptimization(foo);
try { foo() } catch {}
%OptimizeFunctionOnNextCall(foo);
try { foo() } catch {}
