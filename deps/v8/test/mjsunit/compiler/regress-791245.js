// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-gc

var a, b;  // Global variables that will end up with number map.

for (var i = 0; i < 100000; i++) {
  b = 1;
  a = i + -0;  // -0 is a number, so this will make "a" a heap object.
  b = a;
}

assertTrue(a === b);
gc();
assertTrue(a === b);
