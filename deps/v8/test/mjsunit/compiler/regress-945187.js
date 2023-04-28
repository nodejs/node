// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function f() {
  const o = {get: Object};
  Object.defineProperty(Object, 0, o);
};
%PrepareFunctionForOptimization(f);
f();
%OptimizeFunctionOnNextCall(f);
delete Object.fromEntries;
f();
