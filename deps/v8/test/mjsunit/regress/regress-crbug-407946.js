// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function f(n) { return [0].indexOf((n - n) + 0); }

assertEquals(0, f(.1));
assertEquals(0, f(.1));
%OptimizeFunctionOnNextCall(f);
assertEquals(0, f(.1));
