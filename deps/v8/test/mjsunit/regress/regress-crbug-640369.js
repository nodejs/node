// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function A() {
  this.x = 0;
  for (var i = 0; i < max; ) {}
}
function foo() {
  for (var i = 0; i < 1; i = 2) %OptimizeOsr();
  return new A();
}
try { foo(); } catch (e) { }
