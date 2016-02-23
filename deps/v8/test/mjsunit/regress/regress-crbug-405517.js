// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --gc-interval=203

function f() {
 var e = [0];
 %PreventExtensions(e);
 for (var i = 0; i < 4; i++) e.shift();
}

f();
f();
%OptimizeFunctionOnNextCall(f);
f();
