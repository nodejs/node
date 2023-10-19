// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const kMapSizeLimit = 1 << 24;
let m = new Map();
for (let i = 0; i < kMapSizeLimit + 1; i++) {
  m.set(i, 0);
}
