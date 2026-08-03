// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-wasm-shared --no-liftoff --trace-turbo-graph

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

(function Pause() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  builder.addFunction("pause", kSig_v_v)
    .addBody([kAtomicPrefix, kExprPause])
    .exportFunc();

  let instance = builder.instantiate();
  // Test that the ArchPause instruction "survives" until instruction selection
  // (meaning it doesn't get optimized away during any optimization phase).
  // We check this after instruction selection because at this point it is still
  // independent of the architecture.
  // (The main reason to test this is that the pause instruction itself does not
  // provide any observable side effects.)

  // CHECK-LABEL: Begin compiling method pause using Turboshaft
  // CHECK: Instruction sequence after instruction selection
  // CHECK: ArchPause
  // CHECK: Instruction sequence before register allocation
  // CHECK-LABEL: Finished compiling method pause using Turboshaft
  instance.exports.pause();
})();
