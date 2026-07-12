// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax  --fuzzing

// Do not crash on non-JSFunction input when fuzzing.
%NeverOptimizeFunction(undefined);
%NeverOptimizeFunction(true);
%NeverOptimizeFunction(1);
%NeverOptimizeFunction({});
%NeverOptimizeFunction();

%PrepareFunctionForOptimization(print);
%OptimizeFunctionOnNextCall(print);
