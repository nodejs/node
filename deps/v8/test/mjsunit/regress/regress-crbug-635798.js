// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function foo() {
  var x = [];
  var y = [];
  x.__proto__ = y;
  for (var i = 0; i < 10000; ++i) {
    y[i] = 1;
  }
}
foo();
