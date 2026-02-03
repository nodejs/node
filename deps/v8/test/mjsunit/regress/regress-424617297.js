// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --single-threaded --no-maglev

const topLevel = %GetFunctionForCurrentFrame();
%PrepareFunctionForOptimization(topLevel);

for (let v0 = 0; v0 < 5; v0++) {
  %OptimizeOsr();
  for (let v2 = 0; v2 < 5; v2++) {
  }
  ("").charCodeAt(v0);
}
