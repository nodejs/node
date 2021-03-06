// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax

let actual = [];

async function f() {
  var p = Promise.resolve(0);
  Object.defineProperty(p, "constructor", {
    get() {
      throw new Error();
    }
  });
  actual.push("start");
  for await (var x of [p]);
  actual.push("never reached");
}

Promise.resolve(0)
  .then(() => actual.push("tick 1"))
  .then(() => actual.push("tick 2"))

f().catch(() => actual.push("catch"));

%PerformMicrotaskCheckpoint();

assertSame(["start", "tick 1", "tick 2", "catch"].join(), actual.join());
