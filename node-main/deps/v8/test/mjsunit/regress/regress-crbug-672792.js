// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

// Generate a function {f} containing a large array literal of doubles.
eval("function f() { return [" + String("0.1,").repeat(65535) + "] }");

%PrepareFunctionForOptimization(f);

// Running the function once will initialize the boilerplate.
assertEquals(65535, f().length);

// Running the function again will perform cloning.
assertEquals(65535, f().length);

// Running the function as optimized code next.
%OptimizeFunctionOnNextCall(f);
assertEquals(65535, f().length);
