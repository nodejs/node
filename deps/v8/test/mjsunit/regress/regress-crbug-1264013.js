// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --interrupt-budget=1000 --assert-types

function f(a, x) {
  return a[x] + 1;
}
var b = false;
for (var i = 0; i < 1000; ++i) {
    var x = f(b, "bar");
    try {
        if (x != 43) throw "Error: bad result: " + x;
    } catch (e) {}
}
