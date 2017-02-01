// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function foo(o) { return %_ToLength(o.length); }

foo(new Array(4));
foo(new Array(Math.pow(2, 32) - 1));
foo({length: 10});
%OptimizeFunctionOnNextCall(foo);
foo({length: 10});
