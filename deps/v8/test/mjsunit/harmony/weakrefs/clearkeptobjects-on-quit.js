// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-weak-refs

// A newly created WeakRef is kept alive until the end of the next microtask
// checkpoint. V8 asserts that the kept objects list is cleared at the end of
// microtask checkpoints when the microtask policy is auto. Test that d8, which
// uses the auto policy, upholds the assert when manually quitting.
let obj = {};
let wr = new WeakRef(obj);
testRunner.quit();
