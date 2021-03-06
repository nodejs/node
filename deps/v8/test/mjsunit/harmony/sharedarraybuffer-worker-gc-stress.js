// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

load("test/mjsunit/worker-ping-test.js");

let config = {
  numThings: 4,       // size of circular buffer
  numWorkers: 4,      // number of workers
  numMessages: 10000, // number of messages sent to each worker
  allocInterval: 11,  // interval for allocating new things per worker
  traceScript: false, // print the script
  traceAlloc: false,  // print each allocation attempt
  traceIteration: 10, // print diagnostics every so many iterations
  abortOnFail: true,  // kill worker if allocation fails

  AllocThing: function AllocThing(id) {
    return new SharedArrayBuffer(10000);
  },
}

RunWorkerPingTest(config);
