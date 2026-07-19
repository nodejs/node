// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const segmenter = new Intl.Segmenter(undefined, {granularity: 'word'});
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
const thai = "ซิ่ง";
let pos = 0;
for (let i = pos; i < pos + thai.length; i++) {
  assertEquals({segment: "ซิ่ง", index: pos, input, isWordLike: true},
      segments.containing(i));
}
pos += thai.length;

// Test SPACE
assertEquals({segment: " ", index: pos, input, isWordLike: false},
    segments.containing(pos));
pos++;

// Test Latin with modifier
const latin_with_modifier = "Ame\u0301lie";
for (let i = pos; i < pos + latin_with_modifier.length; i++) {
  assertEquals(
      {segment: latin_with_modifier, index: pos, input, isWordLike: true},
      segments.containing(i));
}
pos += latin_with_modifier.length;

// Test Han
const taipei = "台北";
for (let i = pos; i < pos + taipei.length; i++) {
  assertEquals({segment: taipei, index: pos, input, isWordLike: true},
      segments.containing(i));
}
pos += taipei.length;

// Test Surrogate pair
const surrogate = "\uD800\uDCB0";
for (let i = pos; i < pos + surrogate.length; i++) {
  assertEquals({segment: surrogate, index: pos, input, isWordLike: true},
      segments.containing(14));
}
pos += surrogate.length;

// Test SPACE
assertEquals({segment: " ", index: pos, input, isWordLike: false},
    segments.containing(pos));
pos++;

// Test Emoji modifier: U+1F44B U+1F3FB
for (let i = pos; i < pos + waving_hand_light_skin_tone.length; i++) {
  assertEquals({segment: waving_hand_light_skin_tone, index: pos, input,
    isWordLike: false},
    segments.containing(i));
}
pos += waving_hand_light_skin_tone.length;

// Test Emoji modifiers sequence: U+1F468 U+1F3FB U+200D U+1F9B0
for (let i = pos; i < pos + man_light_skin_tone_red_hair.length; i++) {
  assertEquals({segment: man_light_skin_tone_red_hair, index: pos, input,
    isWordLike: false},
    segments.containing(i));
}
pos += man_light_skin_tone_red_hair.length;
