// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --expose-gc --maglev-pretenure-store-values


function foo() {
  var x = {};
  var y = {x:x};
  var obj = {y:y};
  %PretenureAllocationSite(obj);
}

%PrepareFunctionForOptimization(foo);
foo();
foo();
foo();
gc();
foo();
%OptimizeFunctionOnNextCall(foo);
foo();
