// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-gc

function f() {
  return function g() {
    return function h() {}
  }
}

var h = f()();

// Make sure code has been flushed.
for (var i of Array(10)) gc();

f()();
