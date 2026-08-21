// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbofan

function bar(a) {
  var x = a[0];
  return x == undefined;
}

%PrepareFunctionForOptimization(bar);
// Make the keyed load be polymorphic on holey smi and holey fast.
bar([, 2, 3]);
bar([, 'two', 'three']);
bar([, 2, 3]);

%OptimizeFunctionOnNextCall(bar);
bar([, 2, 3]);
// Verify that loading the hole doesn't cause deoptimization.
assertOptimized(bar);
