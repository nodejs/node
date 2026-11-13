// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --strict-termination-checks

let terminated = true;
Promise.resolve().then(() => {
  d8.terminateNow();
  // Reached only when termination is suppressed (e.g. differential fuzzing).
  terminated = false;
});
%PerformMicrotaskCheckpoint();
assertFalse(terminated);
