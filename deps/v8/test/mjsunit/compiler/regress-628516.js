// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function f() {
  var i = 0;
  while (1) {
    if ({}) i = expected[0] == x[0];
    i++;
  }
}

assertThrows(f);
