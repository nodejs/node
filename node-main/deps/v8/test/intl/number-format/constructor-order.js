// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Throws only once during construction.
// Check for all getters to prevent regression.
// Preserve the order of getter initialization.
let getCount = 0;

new Intl.NumberFormat(['en-US'], {
  get localeMatcher() {
    assertEquals(0, getCount++);
  },
  get style() {
    assertEquals(1, getCount++);
  },
  get currency() {
    assertEquals(2, getCount++);
  },
  get currencyDisplay() {
    assertEquals(3, getCount++);
  },
  get minimumIntegerDigits() {
    assertEquals(4, getCount++);
  },
  get minimumFractionDigits() {
    assertEquals(5, getCount++);
  },
  get maximumFractionDigits() {
    assertEquals(6, getCount++);
  },
  get minimumSignificantDigits() {
    assertEquals(7, getCount++);
  },
  get maximumSignificantDigits() {
    assertEquals(8, getCount++);
  },
  get useGrouping() {
    assertEquals(9, getCount++);
  },
});
assertEquals(10, getCount);
