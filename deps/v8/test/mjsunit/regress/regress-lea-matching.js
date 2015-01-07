// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function f(a, b, c) {
  a = a|0;
  b = b|0;
  c = c|0;
  var r = 0;
  r = a + ((b << 1) + c) | 0;
  return r|0;
}

assertEquals(8, f(1, 2, 3));
