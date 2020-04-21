// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let caught_in_gen = false;
async function* catch_gen() {
  try {
    yield 42;
  } catch (e) {
    caught_in_gen = true;
  }
}

(async () => {
  const g = catch_gen();
  await g.next();
  try {
    await g.throw(new Error()); // Should be caught in catch_gen, then catch_gen
                                // completes normally.
  } catch (e) {
    assertUnreachable();
  }
  assertTrue(caught_in_gen);
})();
