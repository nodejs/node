// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --sandbox-testing --disable-in-process-stack-traces
// Flags: --turbolev --allow-natives-syntax

function F0() {
  try { new F0(); } catch (e) {}
}

%PrepareFunctionForOptimization(F0);
new F0();

corrupt(F0, 1494921241);

%OptimizeFunctionOnNextCall(F0);
new F0();


function corrupt(obj) {
  let buffer = new Sandbox.MemoryView(0, 0x100000000);
  let memory = new DataView(buffer);

  let addr = memory.getUint32(Sandbox.getAddressOf(obj) + 16, true) - 1;
  memory.setUint32(addr + 8, 0x42, true);
}
