// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax --harmony-promise-all-settled

let count = 0;
class MyPromise extends Promise {
  static get resolve() {
    count++;
    return super.resolve;
  }
}

MyPromise.allSettled([1, 2, 3, 4, 5]);
assertEquals(1, count);
%PerformMicrotaskCheckpoint();
assertEquals(1, count);

count = 0;
MyPromise.allSettled([
  Promise.resolve(1),
  Promise.resolve(2),
  Promise.reject(3)
]);
assertEquals(1, count);
%PerformMicrotaskCheckpoint();
assertEquals(1, count);
