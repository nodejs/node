// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

const r = /x/;
let counter = 0;

r.exec = () => { counter++; return null; }

function f() {
  r.test("ABcd");
}

f();
assertEquals(1, counter);
%OptimizeFunctionOnNextCall(f);

f();
assertEquals(2, counter);
