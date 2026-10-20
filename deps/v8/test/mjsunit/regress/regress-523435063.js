// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Promise.resolve().then(() => {
  // If the bug is present, this microtask runs.
  // Throwing here ensures the test fails.
  throw new Error("Microtask ran after TerminateExecution");
});

throw {
  toString() {
    d8.terminate();
    while (true) {}
  }
};
