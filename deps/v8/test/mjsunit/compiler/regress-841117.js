// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

var v = 1e9;
function f() { return Math.floor(v / 10); }
assertEquals(1e8, f());
%OptimizeFunctionOnNextCall(f);
assertEquals(1e8, f());
