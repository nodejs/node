// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Test onerror in a worker

const w = new Worker(
  function () {
    postMessage("ready");
    globalThis.onerror = function (msg, source, lineno, colno, ex) {
      postMessage("caught: " + msg);
      return true; // Suppress error
    };
    throw new Error("foo");
  },
  { type: "function" },
);

w.onmessage = function (e) {
  const msg = e.data;
  if (msg === "ready") {
    // Worker started.
  } else if (msg.startsWith("caught:")) {
    assertTrue(
      msg.includes("Uncaught Error: foo"),
      "Unexpected message: " + msg,
    );
    w.terminate();
  } else {
    fail("Unknown message: " + msg);
  }
};
