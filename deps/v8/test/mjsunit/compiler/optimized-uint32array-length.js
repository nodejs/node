// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbofan

var a = new Uint32Array(1);
function len(a) { return a.length; }
%PrepareFunctionForOptimization(len);
assertEquals(1, len(a));
assertEquals(1, len(a));
%OptimizeFunctionOnNextCall(len);
assertEquals(1, len(a));
assertOptimized(len);
