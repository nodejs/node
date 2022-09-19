// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function f() {
}

// Make prototype of f go dictionary-mode.
for (var i = 0; i < 10000; i++) {
  f.prototype["b" + i] = 1;
}

var o = new f();

function access(o, k) {
  return o[k];
}

// Create a thin string.
var p = "b";
p += 10001;

assertEquals(undefined, access(o, p));
assertEquals(undefined, access(o, p));
assertEquals(undefined, access(o, p));
f.prototype[p] = 100;
assertEquals(100, access(o, p));
