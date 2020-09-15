// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turboprop

function foo(e, t) {
    for (var n = [e], s = e.length; s > 0; --s) {}
    for (var s = 0; s < n.length; s++) { t() }
}

var e = 'abc';
function t() {};

%PrepareFunctionForOptimization(foo);
foo(e, t);
foo(e, t);
%OptimizeFunctionOnNextCall(foo);
foo(e, t);
