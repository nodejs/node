// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function f(test, a) {
  var v;
  if (test) {
    v = v|0;
  }
  a[v] = 1;
}
var v = new String();
f(false, v);
f(false, v);

v = new Int32Array(10);
f(true, v);
