// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --stress-concurrent-inlining --expose-fast-api

const a = [];
a[0] = 1;

const fast = new d8.test.FastCAPI();
function f(x) { fast.copy_string(x, new Uint8Array(64 * 1024 * 1024)); }

f();
%PrepareFunctionForOptimization(f);
%OptimizeFunctionOnNextCall(f);
f(a);
