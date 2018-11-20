// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function foo(a, i) { return a[i] + 0.5; }

foo({}, 1);
Array.prototype.unshift(1.5);
assertTrue(Number.isNaN(foo({}, 1)));
%OptimizeFunctionOnNextCall(foo);
assertTrue(Number.isNaN(foo({}, 1)));
