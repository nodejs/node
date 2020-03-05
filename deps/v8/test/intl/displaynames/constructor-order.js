// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-intl-displaynames

// Throws only once during construction.
// Check for all getters to prevent regression.
// Preserve the order of getter initialization.
let getCount = 0;

new Intl.DisplayNames(['en-US'], {
  get localeMatcher() {
    assertEquals(0, getCount++);
  },
  get style() {
    assertEquals(1, getCount++);
  },
  get type() {
    assertEquals(2, getCount++);
  },
  get fallback() {
    assertEquals(3, getCount++);
  },
});
assertEquals(4, getCount);
