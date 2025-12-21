// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --sandbox-testing
let sbx_memory = new DataView(new Sandbox.MemoryView(0, 0x100000000));
let global = new WebAssembly.Global({value: 'i32'}, 0);
let type_address = Sandbox.getAddressOf(global) + 28;
let type_value = sbx_memory.getUint32(type_address, true);
sbx_memory.setUint32(type_address, 0xcd, true);
let crash = global.valueOf();
