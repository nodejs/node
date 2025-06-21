// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Test in Chinese locale there is space between the day and hour field.
let opt = {year: 'numeric', month: '2-digit', day: '2-digit', hour: '2-digit',
  minute: '2-digit', second: '2-digit', hour12: false, timeZone: "UTC"};
let d = new Date("2021-01-27T03:15:04Z");

["zh", "zh-CN", "zh-Hant", "zh-TW", "zh-Hans"].forEach(function(l) {
  // Ensure both 27 (day) and 03  (hour) can be found in the string.
  assertTrue(d.toLocaleString(l, opt).indexOf("27") >= 0);
  assertTrue(d.toLocaleString(l, opt).indexOf("03") >= 0);
  // Ensure there is no case that 27 (day) and 03 (hour) concat together.
  assertEquals(-1, d.toLocaleString(l, opt).indexOf("2703"));
});
