// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var Debug = debug.Debug;

function listener() {}
function f() { [1,2,3].forEach(g) }
function g() { debugger }

%PrepareFunctionForOptimization(f);
f();
f();
Debug.setListener(listener);
%OptimizeFunctionOnNextCall(f)
f();
