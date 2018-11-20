// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --noanalyze-environment-liveness --allow-natives-syntax

var r = /r/;
function f() {
  r[r] = function() {};
}

for (var i = 0; i < 300; i++) {
  f();
  if (i == 150) %OptimizeOsr();
}
