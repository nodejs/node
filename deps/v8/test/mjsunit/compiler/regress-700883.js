// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function add(x) {
 return x + x;
}

add(0);
add(1);
%SetForceInlineFlag(add);

var min = Math.min;
function foo(x) {
 x = x|0;
 let y = add(x ? 800000000000 : NaN);
 return min(y, x);
}

%OptimizeFunctionOnNextCall(foo);
foo();
