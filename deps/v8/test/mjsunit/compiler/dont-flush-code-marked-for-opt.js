// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --opt --allow-natives-syntax --expose-gc --flush-bytecode
// Flags: --stress-flush-bytecode

function foo(a) {}

%PrepareFunctionForOptimization(foo);
foo();
foo();
%OptimizeFunctionOnNextCall(foo);
gc();
foo();
assertOptimized(foo);
