// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-intl-numberformat-unified
// Similar to constructor-order.js but also consider the new options
// in https://tc39-transfer.github.io/proposal-unified-intl-numberformat/

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
  // Begin of new options
  get currencySign() {
    assertEquals(4, getCount++);
  },
  get unit() {
    assertEquals(5, getCount++);
  },
  get unitDisplay() {
    assertEquals(6, getCount++);
  },
  get notation() {
    assertEquals(7, getCount++);
  },
  // End of new options
  get minimumIntegerDigits() {
    assertEquals(8, getCount++);
  },
  get minimumFractionDigits() {
    assertEquals(9, getCount++);
  },
  get maximumFractionDigits() {
    assertEquals(10, getCount++);
  },
  get minimumSignificantDigits() {
    assertEquals(11, getCount++);
  },
  get maximumSignificantDigits() {
    assertEquals(12, getCount++);
  },
  // Begin of new options
  get compactDisplay() {
    assertEquals(13, getCount++);
  },
  // End of new options
  get useGrouping() {
    assertEquals(14, getCount++);
  },
  // Begin of new options
  get signDisplay() {
    assertEquals(15, getCount++);
  },
  // End of new options
});
assertEquals(16, getCount);
