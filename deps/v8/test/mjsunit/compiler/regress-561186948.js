// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --expose-gc

"use strict";

function* gen() {
  yield 1;
}

function foo(x) {
  return [gen.call(x)];
}

%PrepareFunctionForOptimization(gen);
%PrepareFunctionForOptimization(foo);

let arr = foo(1);
%PretenureAllocationSite(arr);
gc({type: 'minor'});
foo(1);

%OptimizeFunctionOnNextCall(foo);
foo(-1);
