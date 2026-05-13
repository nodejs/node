// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --sandbox-testing

function f(x) {
  let y = x;
  %WasmTraceMemory(y);
}
f(0);
// CHECK: # The following harmless error was encountered
// CHECK-NOT: Did not crash.
print("Did not crash.")
