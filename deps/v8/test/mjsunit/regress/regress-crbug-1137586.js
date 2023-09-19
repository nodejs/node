// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function f() {
  for (var i = 0; i < 100000; i++) {
    var a = arguments[0] + 2;
    var b = arguments[1] + 2;
    var c = a + i + 5;
    var d = c + 3;
  }
}

for (var j = 0; j < 3; j++) {
  f(2, 3);
}
