// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-gc

const set = new WeakSet()
const obj = {};
// Two GCs to promote {set} and {obj} to old-space.
gc();
gc();
// Allocate a large array so {obj} will become an evacuation candidate.
const foo = new Int8Array(0x0F000000);
// Trigger ephemeron key write barrier.
set.add(obj);
