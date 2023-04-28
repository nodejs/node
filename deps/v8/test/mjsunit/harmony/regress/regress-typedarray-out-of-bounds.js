// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var a = new Int32Array(10);
function f(a) { a["-1"] = 15; }
for (var i = 0; i < 3; i++) {
  f(a);
}
assertEquals(undefined, a[-1]);
