// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

d8.file.execute("test/mjsunit/worker-ping-test.js");

// TODO(v8:9380): increase {numThings} and {numWorkers} when stress-opt mode
// no longer leaks wasm memories.
let config = {
  numThings: 2,         // size of circular buffer
  numWorkers: 2,        // number of workers
  numMessages: 2000,    // number of messages sent to each worker
  allocInterval: 10001, // interval for allocating new things per worker
  traceScript: false,   // print the script
  traceAlloc: true,     // print each allocation attempt
  traceIteration: 10,   // print diagnostics every so many iterations
  abortOnFail: true,    // kill worker if allocation fails

  AllocThing: function AllocThing(id) {
    let pages = 1, max = 1;
    return new WebAssembly.Memory({initial : pages, maximum : max, shared : true});
  },
}

RunWorkerPingTest(config);
