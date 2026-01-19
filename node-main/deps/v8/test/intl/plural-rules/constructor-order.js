// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Throws only once during construction.
// Check for all getters to prevent regression.
// Preserve the order of getter initialization.
let getCount = 0;

new Intl.PluralRules(['en-US'], {
  get localeMatcher() {
    assertEquals(0, getCount++);
  },
  get type() {
    assertEquals(1, getCount++);
  },
  get minimumIntegerDigits() {
    assertEquals(2, getCount++);
  },
  get minimumFractionDigits() {
    assertEquals(3, getCount++);
  },
  get maximumFractionDigits() {
    assertEquals(4, getCount++);
  },
  get minimumSignificantDigits() {
    assertEquals(5, getCount++);
  },
  get maximumSignificantDigits() {
    assertEquals(6, getCount++);
  },
});
assertEquals(7, getCount);
