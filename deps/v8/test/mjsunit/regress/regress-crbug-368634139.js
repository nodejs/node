// Copyright 2024 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --stress-gc-during-compilation --log-code
// Flags: --stress-compaction --interpreted-frames-native-stack

function f() {
  function t() {}
  return t();
}

%PrepareFunctionForOptimization(f);
f();
%OptimizeFunctionOnNextCall(f);
f();
