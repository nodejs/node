// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

try {
  %OptimizeFunctionOnNextCall(print);
} catch(e) { }

try {
  function* f() {
  }
  %OptimizeFunctionOnNextCall(f);
} catch(e) { }
