// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --maglev --allow-natives-syntax

var func = function(){}
function foo() {
    let call = func.call;
    call();
}
%PrepareFunctionForOptimization(foo);
try {foo();} catch {}
try {foo();} catch {}
%OptimizeMaglevOnNextCall(foo);
try {foo();} catch {}
