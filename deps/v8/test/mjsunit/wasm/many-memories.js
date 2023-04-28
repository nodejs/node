// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

// Test that we can generate at least 50 memories of small size.
// More memories are currently not possible if the trap handler is enabled,
// because we reserve 10GB then, and we have a virtual memory space limit of
// 512GB on MIPS64 and 1TB+4GB on other 64-bit systems.

// The number of memories should be increased in this test once we raise that
// limit or fix the allocation strategy to allow for more memories generally.

const num_memories = 50;

const memories = [];
while (memories.length < num_memories) {
  print('Allocating memory #' + memories.length);
  memories.push(new WebAssembly.Memory({initial: 1, maximum: 1}));
}
