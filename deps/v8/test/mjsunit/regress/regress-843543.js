// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax

const o = {x:9};
o.__proto__ = Array.prototype;

function foo(o) {
  return o.indexOf(undefined);
}

assertEquals(-1, foo(o));
assertEquals(-1, foo(o));
%OptimizeFunctionOnNextCall(foo);
assertEquals(-1, foo(o));
