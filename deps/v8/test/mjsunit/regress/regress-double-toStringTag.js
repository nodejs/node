// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function make() {
  var v = { maxByteLength: 1073741824 };
  Object.defineProperty(v, "p1", { value: 0 });
  Object.defineProperty(v, "p2", { value: 0 });
  Object.defineProperty(v, Symbol.toStringTag, { value: 1073741824 });
  return v;
}

// Keep it alive in a variable so it is visited during the snapshot.
const keep = make();

// Trigger heap snapshot
%TakeHeapSnapshot("/dev/null");
