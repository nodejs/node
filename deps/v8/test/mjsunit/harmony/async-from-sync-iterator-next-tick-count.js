// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax

const actual = [];

function createIterator() {
  return {
    [Symbol.iterator]() { return this; },

    next() {
      return { value: Promise.resolve(1), done: true};
    }
  };
}

actual.push("start");
iter = %CreateAsyncFromSyncIterator(createIterator());

Promise.resolve(0)
  .then(() => actual.push("tick 1"))
  .then(() => actual.push("tick 2"))
  .then(() => actual.push("tick 3"))
  .then(() => actual.push("tick 4"))
  .then(() => actual.push("tick 5"));

let p = iter.next();
p.then(_ => { actual.push("done"); } );

%PerformMicrotaskCheckpoint();
assertSame("start,tick 1,tick 2,done,tick 3,tick 4,tick 5", actual.join(","))
