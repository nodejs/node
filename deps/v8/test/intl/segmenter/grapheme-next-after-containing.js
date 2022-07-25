// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Test to ensure the calling of containing() won't impact the calling of
// the next() method.
const segmenter = new Intl.Segmenter();
const man_light_skin_tone_red_hair =
    "\uD83D\uDC68\uD83C\uDFFB\u200D\uD83E\uDDB0";
const input = "ABCD" + man_light_skin_tone_red_hair;
const segments = segmenter.segment(input);
for (let i = 0; i < input.length; i++) {
  let idx = i < 4 ? i : 4;
  let expectation = i < 4 ? input[i] : man_light_skin_tone_red_hair;
  assertEquals({segment: expectation, index: idx, input},
      segments.containing(i));
  let result = [];
  for (let v of segments) {
    result.push(v.segment);
    result.push(":");
    // Ensure the value n passing into segments.containing(n) will not impact
    // the result of next().
    assertEquals({segment: expectation, index: idx, input},
        segments.containing(i));
  }
  assertEquals("A:B:C:D:" + man_light_skin_tone_red_hair + ":",
      result.join(""));
}
