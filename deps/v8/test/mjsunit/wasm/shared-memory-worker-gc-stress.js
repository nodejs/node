// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-wasm-threads

load("test/mjsunit/worker-ping-test.js");

let kDisabledAbort = false; // TODO(9380): enable abort for this test

let config = {
  numThings: 4,       // size of circular buffer
  numWorkers: 4,      // number of workers
  numMessages: 1000,  // number of messages sent to each worker
  allocInterval: 11,  // interval for allocating new things per worker
  traceScript: false, // print the script
  traceAlloc: true,   // print each allocation attempt
  traceIteration: 10, // print diagnostics every so many iterations
  abortOnFail: kDisabledAbort,  // kill worker if allocation fails

  AllocThing: function AllocThing(id) {
    let pages = 1, max = 1;
    return new WebAssembly.Memory({initial : pages, maximum : max, shared : true});
  },
}

RunWorkerPingTest(config);
