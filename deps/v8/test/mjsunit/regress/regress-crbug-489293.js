// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbo-filter=f
// Flags: --noanalyze-environment-liveness

function f() {
  var x = 0;
  for (var y = 0; y < 0; ++y) {
    x = (x + y) | 0;
  }
  return unbound;
}
%OptimizeFunctionOnNextCall(f);
assertThrows(f, ReferenceError);
