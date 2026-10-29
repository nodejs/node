// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


function body() {
    // Signal worker has started.
    this.postMessage(true);
    // Keep the worker running forever
    while (true) {
        new WeakRef([]);
    }
}
const worker = new Worker(body, { type: "function" });

// Wait for the worker to start.
const workerIsRunning = worker.getMessage();
// d8 will terminate the workers after running the main script here.
