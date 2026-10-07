// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --expose-gc
// Flags: --turbolev --no-lazy-feedback-allocation

function wrap(f, permissive = true) {
  try {
    return f();
  } catch (e) {
  }
}
%PrepareFunctionForOptimization(wrap);

function f1(useSmi, returnEarly) {
  if (returnEarly) return;
  let obj = { a: 1 };
  let arrow = () => useSmi ? 42 : obj;
  // Important that we don't do %PrepareFunctionForOptimization(arrow) here
  let objOrSmi = wrap(arrow);
  objOrSmi | -6;
  obj.a = objOrSmi; // This is the offending store
  return obj;
}
f1(true);

%PretenureAllocationSite(f1(true));

gc({});

function f2(returnEarly) {
  return f1(false, returnEarly);
}
%PrepareFunctionForOptimization(f2);
f2(true);
%OptimizeFunctionOnNextCall(f2);
f2(true);
