// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --enable-inspector

const Debug = debug.Debug;
Debug.setListener(() => {});
Debug.setBreakOnUncaughtException()

function sleep() {
  return new Promise(resolve => setTimeout(resolve, 1));
}
async function thrower() {
  await sleep();
  throw "a"; // Exception a
}
(async function() { thrower(); })();
