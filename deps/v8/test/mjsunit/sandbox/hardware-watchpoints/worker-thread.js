// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --memory-corruption-via-watchpoints --invoke-weak-callbacks

// Arm a watchpoint on a non-tagged payload field on the main thread so
// `g_support.kept_alive` is non-empty while the worker executes.
let n = 1.1;
Sandbox.markForCorruptionOnAccess(n, 4);

let worker = new Worker(
    function() {
      let error_message = null;
      try {
        Sandbox.markForCorruptionOnAccess({a: 1}, 0);
      } catch (e) {
        if (e instanceof TypeError) {
          error_message = e.message;
        }
      }
      // Exercise Heap::CanMoveObjectStart -> SandboxTesting::IsWatchedObject
      // on a non-main thread while the main thread has an active watchpoint.
      let arr = new Array(200).fill(1);
      arr.shift();
      postMessage(error_message);
    },
    {type: 'function'});

assertEquals(
    'Hardware watchpoints are only supported on the main thread',
    worker.getMessage());
worker.terminateAndWait();
