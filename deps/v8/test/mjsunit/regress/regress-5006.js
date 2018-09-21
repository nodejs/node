// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function foo(x) { return Math.imul(x|0, 2); }
print(foo(1));
print(foo(1));
%OptimizeFunctionOnNextCall(foo);
print(foo(1));
