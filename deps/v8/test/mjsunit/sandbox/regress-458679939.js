// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --sandbox-testing

function foo(a, b, c, ...rest) {
  return rest[0];
}
%PrepareFunctionForOptimization(foo);
foo(1, 2, 3, 4, 5);
foo(1, 2, 3, 4, 5);
%OptimizeFunctionOnNextCall(foo);

let memory = new DataView(new Sandbox.MemoryView(0, 0x100000000));
let foo_offset = Number(Sandbox.getAddressOf(foo));
let sfi_compressed = memory.getUint32(foo_offset + 16, true);
let sfi_offset = sfi_compressed & ~1;
let corrupted_formal_parameter_count = 0xffff;
memory.setUint16(sfi_offset + 26, corrupted_formal_parameter_count, true);

foo(1, 2, 3, 4, 5);
