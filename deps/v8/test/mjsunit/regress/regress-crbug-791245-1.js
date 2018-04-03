// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

const s = new Map;

function foo(s) {
  const i = s[Symbol.iterator]();
  i.next();
  return i;
}

console.log(foo(s));
console.log(foo(s));
%OptimizeFunctionOnNextCall(foo);
console.log(foo(s));
