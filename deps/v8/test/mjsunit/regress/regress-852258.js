// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

try {
  let ta0 = new Int16Array(0x24924925);
  let ta2 = ta0.slice(1);
  let ta1 = ta0.slice(0x24924924);
} catch (e) {
  // Allocation failed, that's fine.
}
