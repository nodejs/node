// Copyright 2024 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax

class Class {}
const obj_with_hasInstance = {}
obj_with_hasInstance[Symbol.hasInstance] = Class;

function foo(x) {
  x instanceof obj_with_hasInstance;
}

%PrepareFunctionForOptimization(foo);
try { foo(); } catch {}
%OptimizeMaglevOnNextCall(foo);
try { foo(); } catch {}
