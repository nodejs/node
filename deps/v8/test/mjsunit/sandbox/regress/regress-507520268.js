// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --sandbox-testing --verify-bytecode-full

function f() {}
f();
let ba = %GetBytecode(f);
let id = 613; // Runtime::kWasmAllocateFeedbackVector
let new_bc = new Uint8Array([
  0x70, id & 0xff,id >> 8 & 0xff, 0xf9, 0x03, // CallRuntime 613, r0-r2, 3 args
   0xb9
]);
ba.bytecode = new_bc.buffer;
ba.frame_size = 24;
%InstallBytecode(f, ba);

f();

assertUnreachable("Process should have been killed.");
