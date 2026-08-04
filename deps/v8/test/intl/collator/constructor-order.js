// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Throws only once during construction.
// Check for all getters to prevent regression.
// Preserve the order of getter initialization.
let getCount = 0;

new Intl.Collator(['en-US'], {
  get usage() {
    assertEquals(0, getCount++);
  },
  get localeMatcher() {
    assertEquals(1, getCount++);
  },
  get collation() {
    assertEquals(2, getCount++);
  },
  get numeric() {
    assertEquals(3, getCount++);
  },
  get caseFirst() {
    assertEquals(4, getCount++);
  },
  get sensitivity() {
    assertEquals(5, getCount++);
  },
  get ignorePunctuation() {
    assertEquals(6, getCount++);
  },
});
assertEquals(7, getCount);
