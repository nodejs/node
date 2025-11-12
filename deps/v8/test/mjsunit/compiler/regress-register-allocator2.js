// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function f() {
  var x = 0;
  var y = 0;
  x ^= undefined;
  assertEquals(x /= 1);
  assertEquals(NaN, y %= 1);
  assertEquals(y = 1);
  f();
  y = -2;
  assertEquals(x >>= 1);
  assertEquals(0, ((y+(y+(y+((y^(x%5))+y)))+(y+y))>>y)+y);
}
try { f(); } catch (e) {}
