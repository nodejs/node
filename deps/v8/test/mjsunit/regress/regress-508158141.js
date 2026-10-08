// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --for-of-optimization

function* mygen() {
  let items = [1, 2,,];
  for (let item of items) {
    yield 0;
  }
}

let getterCalled = false;
function mygetter() {
  getterCalled = true;
}

Object.defineProperty(Array.prototype, "2", {
  get: mygetter,
});

let g = mygen();
g.next();
g.next();
g.next();

assertTrue(getterCalled);
