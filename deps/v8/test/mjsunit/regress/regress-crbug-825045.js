// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

const obj = new class A extends (async function (){}.constructor) {};
delete obj.name;
Number.prototype.__proto__ = obj;
function foo() { return obj.bind(); }
foo();
foo();
%OptimizeFunctionOnNextCall(foo);
foo();
