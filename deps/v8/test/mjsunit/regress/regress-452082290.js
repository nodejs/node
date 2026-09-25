// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --for-of-optimization

const arr = [1];
const iter = arr[Symbol.iterator]();

iter.next();
iter.next();

for (const _ of iter) {
  throw new Error('Should not reach here!');
}
