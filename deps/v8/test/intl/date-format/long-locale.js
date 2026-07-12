// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let locale = "zh-TW-u-ca-chinese";
for (let i = 0; i < 300; ++i) {
  try {
    Intl.DateTimeFormat(locale);
  } catch (e) {
    // "RangeError: Invalid language tag", for locales ending with "-", or
    // sub-tags of one character, are not relevant to this test.
  }
  locale += (i % 5) ? "a" : "-";
}
// Pass if this test doesn't crash.
