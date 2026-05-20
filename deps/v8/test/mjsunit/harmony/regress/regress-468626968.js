// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --js-iterator-sequencing
// Flags: --allow-natives-syntax --fuzzing --jit-fuzzing

const v = Iterator.concat();
let count = 0;
function f() { if (count++ == 0) f(); v; }

f();
%OptimizeFunctionOnNextCall(f);
f();
