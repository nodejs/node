// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var g = 0;
g++;
var f32 = new Float32Array();
function foo() {
  f32[0 >> 2] = g;
}
foo();
