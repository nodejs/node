// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --expose-gc --harmony-struct

let mtx = new Atomics.Mutex();
let cnd = new Atomics.Condition();

Atomics.Mutex.lock(mtx, () => {
    Atomics.Condition.wait(cnd, mtx, 100);
});

gc();
gc();

Atomics.Mutex.lock(mtx, () => {
    Atomics.Condition.wait(cnd, mtx, 100);
});
