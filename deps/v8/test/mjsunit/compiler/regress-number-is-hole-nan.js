// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

var a = [, 2.121736758e-314];

function foo() { return a[1]; }

assertEquals(2.121736758e-314, foo());
assertEquals(2.121736758e-314, foo());
%OptimizeFunctionOnNextCall(foo);
assertEquals(2.121736758e-314, foo());
