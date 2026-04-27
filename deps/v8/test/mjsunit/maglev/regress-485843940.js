// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev-assert-types --maglev --single-threaded

const topLevel = %GetFunctionForCurrentFrame();
%PrepareFunctionForOptimization(topLevel);

let limit = 1;
for (let i = limit; i >= limit; i += limit) {
  const stuff = i++;
  %OptimizeOsr();
  stuff.length;
  limit = limit && i;
  if (i > 805306367) break; // for terminating the loop if the test passes
}
