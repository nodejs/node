// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

try {
%OptimizeFunctionOnNextCall(print);
try {
  __f_16();
} catch(e) { print(e); }
try {
  __f_10();
} catch(e) {; }
} catch(e) {}
