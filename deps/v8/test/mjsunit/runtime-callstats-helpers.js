// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

// Add --allow-natives-syntax --runtime-call-stats to your test file in order to
// use this function. You can suppress the extra printout by calling
// %GetAndResetRuntimeCallStats() at the end of the test.
function getRuntimeFunctionCallCount(function_name) {
  const stats = %GetAndResetRuntimeCallStats();
  const lines = stats.split("\n");
  for (let i = 3; i < lines.length - 3; ++i) {
    const line = lines[i];
    const m = line.match(/(?<name>\S+)\s+\S+\s+\S+\s+(?<count>\S+)/);
    if (function_name == m.groups.name) {
      return m.groups.count;
    }
  }
  return 0;
}
