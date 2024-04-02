// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --shared-string-table --harmony-struct --allow-natives-syntax

"use strict";

if (this.Worker) {

(function TestSharedStructPostMessage() {
  let workerScript =
      `onmessage = function(struct) {
         // Non-atomic write that will be made visible once main thread
         // observes the atomic write below.
         struct.struct_field.payload = 42;
         Atomics.store(struct, "string_field", "worker");
       };
       postMessage("started");`;

  let worker = new Worker(workerScript, { type: 'string' });
  let started = worker.getMessage();
  assertEquals("started", started);

  let OuterStruct = new SharedStructType(['string_field', 'struct_field']);
  let InnerStruct = new SharedStructType(['payload']);
  let struct = new OuterStruct();
  struct.struct_field = new InnerStruct();
  struct.string_field = "main";
  assertEquals("main", struct.string_field);
  assertEquals(undefined, struct.struct_field.payload);
  worker.postMessage(struct);
  // Spin until we observe the worker's write of string_field.
  while (Atomics.load(struct, "string_field") !== "worker") {}
  // The non-atomic store write must also be visible.
  assertEquals(42, struct.struct_field.payload);

  worker.terminate();
})();

}
