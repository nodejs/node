// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Throws only once during construction.
// Check for all getters to prevent regression.
// Preserve the order of getter initialization.
let getCount = 0;

new Intl.Segmenter(['en-US'], {
  get localeMatcher() {
    assertEquals(0, getCount++);
  },
  get granularity() {
    assertEquals(1, getCount++);
  },
});
assertEquals(2, getCount);
