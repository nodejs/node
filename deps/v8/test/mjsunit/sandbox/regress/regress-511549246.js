// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --sandbox-fuzzing

const workerArguments = [];
// Corrupt the length field of the JSArray to expose out-of-bounds elements
// (the_hole). Writing raw integer 8 (Smi tagged internally as 4) extends
// length beyond capacity.
Sandbox.corruptObjectField(workerArguments, "length", 8);

function emptyWorker() {}

try {
  new Worker(emptyWorker, {
    arguments: workerArguments,
    type: "function"
  });
} catch (e) {
  // May throw validation error depending on adjacent heap contents.
}
