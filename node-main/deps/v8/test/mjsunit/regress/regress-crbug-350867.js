// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function f1(a, i) {
  return a[i];
}
function f2(a, b, c, index) {
  return f1(arguments, index);
}

f2(2, 3, 4, "foo");
f2(2, 3, 4, "foo");
assertEquals(11, f1([11, 22, 33], 0));
assertEquals(22, f2(22, 33, 44, 0));
