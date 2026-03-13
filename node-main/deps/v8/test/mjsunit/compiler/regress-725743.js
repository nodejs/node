// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --no-turbo --cache=code --no-lazy

function f() {
  var n = a.length;
  for (var i = 0; i < n; i++) {
  }
  for (var i = 0; i < n; i++) {
  }
}
var a = "xxxxxxxxxxxxxxxxxxxxxxxxx";
while (a.length < 100000) a = a + a;
f();
