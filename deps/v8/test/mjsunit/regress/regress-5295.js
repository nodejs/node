// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

eval('var x; let y; ()=>y');
assertEquals(undefined, x);

function foo() {
  eval('var z = 1; let w; ()=>w');
  return z;
}
assertEquals(1, foo());

// Multiply nested eval hoisting works

eval('let a; ()=>a; eval("let b; ()=>b; var c; function d() {}")');
assertEquals(undefined, c);
assertEquals("d", d.name);
