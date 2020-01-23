// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --noexperimental-wasm-threads --allow-natives-syntax

load("test/mjsunit/wasm/wasm-module-builder.js");

function instantiateModuleWithThreads() {
  // Build a WebAssembly module which uses threads-features.
  const builder = new WasmModuleBuilder();
  const shared = true;
  builder.addMemory(2, 10, false, shared);
  builder.addFunction('main', kSig_i_ii)
      .addBody([
        kExprLocalGet, 0, kExprLocalGet, 1, kAtomicPrefix, kExprI32AtomicAdd, 2,
        0
      ])
      .exportFunc();

  return builder.instantiate();
}

// Disable WebAssembly threads initially.
%SetWasmThreadsEnabled(false);
assertThrows(instantiateModuleWithThreads, WebAssembly.CompileError);

// Enable WebAssembly threads.
%SetWasmThreadsEnabled(true);
assertInstanceof(instantiateModuleWithThreads(), WebAssembly.Instance);

// Disable WebAssembly threads.
%SetWasmThreadsEnabled(false);
assertThrows(instantiateModuleWithThreads, WebAssembly.CompileError);
