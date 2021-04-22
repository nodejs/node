// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --no-wasm-trap-handler

// No reason to stress-opt this; save some time.
// Flags: --no-stress-opt

load('test/mjsunit/wasm/wasm-module-builder.js');

// Without trap handlers, we are able to allocate basically arbitrarily many
// memories, because we don't need to reserve a huge amount of virtual address
// space.

const num_memories = 10000;

const memories = [];
while (memories.length < num_memories) {
  print('Allocating memory #' + memories.length);
  memories.push(new WebAssembly.Memory({initial: 1, maximum: 1}));
}
