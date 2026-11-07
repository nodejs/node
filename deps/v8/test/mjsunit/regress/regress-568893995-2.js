// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

let worker = new Worker(`
  const iterator = {
    [Symbol.iterator]() {
      return this;
    },
    next() {
      return { value: 42, done: false };
    },
    return() {
      postMessage("return_called");
      return {};
    }
  };
  %IterableForEach(iterator, () => {
    d8.terminateNow();
  });
`, {type: "string"});

let msg = worker.getMessage();
assertEquals(undefined, msg,
  "iterator.return() must not be called when execution is terminating");
