// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var v = [];

(async () => {
  for (let i = 0; i < 6; ++i) {
    v.push(i);
    await 0;
  }
})();

async function TestCStyleForCountTicks() {
  for (await using x = {
         value: 42,
         [Symbol.asyncDispose]() {
           v.push(`asyncDispose`);
         }  // One tick is expected after calling asyncDispose to allow it to be
            // asynchronous. It will be called after exiting the for-loop.
       };
       x.value < 44; x.value++) {
    // These pushes are expected to be synchronous.
    v.push(x.value);
  }
  v.push(`afterForLoop`);
}

async function RunTest() {
  await TestCStyleForCountTicks();
  assertArrayEquals([0, 42, 43, `asyncDispose`, 1, `afterForLoop`, 2], v);
}

RunTest();
