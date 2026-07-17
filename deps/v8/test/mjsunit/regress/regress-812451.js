// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-gc

var x = [];
function foo(x, p) {
  x[p] = 5.3;
}
foo(x, 1);
foo(x, 2);
foo(x, -1);
gc();
