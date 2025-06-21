// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const kSetSizeLimit = 1 << 24;
let s = new Set();
for (let i = 0; i < kSetSizeLimit + 1; i++) {
  s.add(i);
}
