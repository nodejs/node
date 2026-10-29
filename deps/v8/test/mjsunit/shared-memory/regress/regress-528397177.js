// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-gc --shared-string-table --cache

const value = "some value";
const map = new WeakMap();
const key = () => {};
map.set(key, value);
gc({
  type: 'major-snapshot',
});
