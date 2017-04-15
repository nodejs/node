// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --ignition-staging --turbo

function f(do_osr) {
  for (var i = 0; i < 3; ++i) {
    if (i == 1 && do_osr) %OptimizeOsr();
  }
}

f(false);
f(false);
%BaselineFunctionOnNextCall(f);
f(false);
f(true);
