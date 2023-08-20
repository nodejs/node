// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const segmenter = new Intl.Segmenter();
const waving_hand_light_skin_tone = "\uD83D\uDC4B\uD83C\uDFFB";
const man_light_skin_tone_red_hair =
    "\uD83D\uDC68\uD83C\uDFFB\u200D\uD83E\uDDB0";
//                          1                                  2
//             034566     89012 3      4     56    89          01
const input = "ซิ่ง Ame\u0301lie台北\uD800\uDCB0 " + waving_hand_light_skin_tone +
//  2
//  2345678
    man_light_skin_tone_red_hair;
const segments = segmenter.segment(input);
// Test less than 0
assertEquals(undefined, segments.containing(-1));

// Test longer than input
assertEquals(undefined, segments.containing(input.length));
assertEquals(undefined, segments.containing(input.length + 1));

// Test modifier in Thai
assertEquals({segment: "ซิ่", index: 0, input}, segments.containing(0));
assertEquals({segment: "ซิ่", index: 0, input}, segments.containing(1));
assertEquals({segment: "ซิ่", index: 0, input}, segments.containing(2));

// Test basic Thai
assertEquals({segment: "ง", index: 3, input}, segments.containing(3));

// Test SPACE
assertEquals({segment: " ", index: 4, input}, segments.containing(4));
// Test ASCII
assertEquals({segment: "A", index: 5, input}, segments.containing(5));
assertEquals({segment: "m", index: 6, input}, segments.containing(6));

// Test ASCII with modifier
assertEquals({segment: "e\u0301", index: 7, input}, segments.containing(7));
assertEquals({segment: "e\u0301", index: 7, input}, segments.containing(8));

// Test ASCII
assertEquals({segment: "l", index: 9, input}, segments.containing(9));
assertEquals({segment: "i", index: 10, input}, segments.containing(10));
assertEquals({segment: "e", index: 11, input}, segments.containing(11));

// Test Han
assertEquals({segment: "台", index: 12, input}, segments.containing(12));
assertEquals({segment: "北", index: 13, input}, segments.containing(13));

// Test Surrogate pairs
assertEquals({segment: "𐂰", index: 14, input}, segments.containing(14));
assertEquals({segment: "𐂰", index: 14, input}, segments.containing(15));

// Test SPACE
assertEquals({segment: " ", index: 16, input}, segments.containing(16));

// Test Emoji modifier: U+1F44B U+1F3FB
const emoji1 = {segment: waving_hand_light_skin_tone, index: 17, input};
assertEquals(emoji1, segments.containing(17));
assertEquals(emoji1, segments.containing(18));
assertEquals(emoji1, segments.containing(19));
assertEquals(emoji1, segments.containing(20));

// Test Emoji modifiers sequence: U+1F468 U+1F3FB U+200D U+1F9B0
const emoji2 = {segment: man_light_skin_tone_red_hair, index: 21, input};
assertEquals(emoji2, segments.containing(21));
assertEquals(emoji2, segments.containing(22));
assertEquals(emoji2, segments.containing(23));
assertEquals(emoji2, segments.containing(24));
assertEquals(emoji2, segments.containing(25));
assertEquals(emoji2, segments.containing(26));
assertEquals(emoji2, segments.containing(27));
