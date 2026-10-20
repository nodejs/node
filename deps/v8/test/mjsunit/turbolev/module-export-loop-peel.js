// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev

let m;
import(
    `data:text/javascript,

     export let value = 0;
     export function set(x) { value = x; };
     export let a = 11;
`).then(mod => {
  m = mod;
});
%PerformMicrotaskCheckpoint();

function foo() {
  for (let i = 0; i < 5; i++) {
    let x = m.value;
    x += m.a;
    m.set(x);
  }
}

function check() {
  assertEquals(55, m.value);
  m.set(0);
}

%PrepareFunctionForOptimization(foo);
foo();
check();
foo();
check();
%OptimizeFunctionOnNextCall(foo);
foo();
check();
assertOptimized(foo);
