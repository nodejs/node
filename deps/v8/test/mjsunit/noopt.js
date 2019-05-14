// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --noopt --no-always-opt


// Check that --noopt actually works.

function f() { return 42; }

f();
f();
%OptimizeFunctionOnNextCall(f);
f();

assertUnoptimized(f);
