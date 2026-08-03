// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

// Case One: the template is already initialized before compilation.
let saved_array;
function tagged_function(a) {
  saved_array = a;
  return "something";
}

function foo(b) {
  let a = tagged_function`hello ${b}`;
  return a + " " + b;
}

%PrepareFunctionForOptimization(foo);
foo();
foo();
%OptimizeFunctionOnNextCall(foo);
foo();

// Case Two: the template hasn't been initialized at the point we
// do optimized compile.
function bar(b) {
  if (b) {
    let a = tagged_function`initialized late ${b}`;
    b = a;
  }
  return b;
}

%PrepareFunctionForOptimization(bar);
bar();
bar();
%OptimizeFunctionOnNextCall(bar);
bar(true);

let saved_array_from_optimized_call = saved_array;

// Finally, ensure that if the function is deoptimized, the tagged-template
// code still runs. This is useful to test because TurboFan doesn't cache
// the tagged template in the feedback vector if it has to create it.
%DeoptimizeFunction(bar);
bar(true);

// Furthermore, we want to ensure that the JSArray passed to the function
// is the same.
assertSame(saved_array_from_optimized_call, saved_array);
