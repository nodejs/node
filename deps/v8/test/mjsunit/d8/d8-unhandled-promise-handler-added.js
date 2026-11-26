// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let handled = 0;

const p1 = Promise.reject(new Error('first'));
const p2 = Promise.reject(new Error('second'));
const p3 = Promise.reject(new Error('third'));

// Attaching handlers after rejection triggers kPromiseHandlerAddedAfterReject
// -> PerIsolateData::RemoveUnhandledPromise, including removing the front
// element of `unhandled_promises_`.
p1.catch(() => {
  handled++;
});
p3.catch(() => {
  handled++;
});
p2.catch(() => {
  handled++;
});

setTimeout(() => {
  assertEquals(3, handled);
}, 0);
