// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function f(a, i, v) {
  a[i] = v;
}

f("make it generic", 0, 0);

var a = new Array(3);
// Fast properties.
f(a, "length", 2);
assertEquals(2, a.length);

// Dictionary properties.
%OptimizeObjectForAddingMultipleProperties(a, 1);
f(a, "length", 1);
assertEquals(1, a.length);
