// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --expose-gc

function p() { return { a: /z/ }; }
%PrepareFunctionForOptimization(p);
p();
%OptimizeFunctionOnNextCall(p);
// Compile with AllocationSite now, so that we can pretenure.
let o1 = p();
%PretenureAllocationSite(o1);
// Pretenure allocation and deoptimize p().
gc({type: 'minor'});
// Compile p() again with pretenured allocation.
%OptimizeFunctionOnNextCall(p);
// p() now has initializing indirect pointer store to old allocation.
p();
