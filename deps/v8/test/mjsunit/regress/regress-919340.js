// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --opt

var E = 'Σ';
var PI = 123;
function f() {
    print(E = 2, /b/.test(E) || /b/.test(E = 2));
    ((E = 3) * PI);
}

f();
f();
%OptimizeFunctionOnNextCall(f);
f();
