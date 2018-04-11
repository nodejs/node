// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function foo(p) { return p.catch(x => x); }

const a = Promise.resolve(1);

foo(a);
foo(a);
%OptimizeFunctionOnNextCall(foo);
foo(a);

let custom_then_called = false;
a.__proto__.then = function() { custom_then_called = true; }
foo(a);
assertTrue(custom_then_called);
