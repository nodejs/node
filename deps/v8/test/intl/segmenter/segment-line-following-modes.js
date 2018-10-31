// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-intl-segmenter

let breakCounts = {};
for (const locale of ["en", "fr", "ja", "zh", "ko"]) {
  for (const lineBreakStyle of ["strict", "normal", "loose"]) {
    const seg = new Intl.Segmenter(
        [locale], {granularity: "line", lineBreakStyle: lineBreakStyle});
    let opportunity = 0;
    for (const text of [
      // We know the following data caused different line break results between
      // different modes.
      // https://www.w3.org/TR/css-text-3/#propdef-line-break
      // Japanese small kana or the Katakana-Hiragana prolonged sound mark
      "あぁーぃーあーいーぁーぃー",
      // hyphens:
      // ‐ U+2010, – U+2013, 〜 U+301C, ゠ U+30A0
      "ABC‐DEF–GHI〜JKL゠MNO",
      // iteration marks:
      // 々 U+3005, 〻 U+303B, ゝ U+309D, ゞ U+309E, ヽ U+30FD, ヾ U+30FE
      "あ々あ〻あゝあゞあヽあヾあ",
      // centered punctuation marks:
      // ・ U+30FB, ： U+FF1A, ； U+FF1B, ･ U+FF65, ‼ U+203C
      "ABC・DEF：GHI；JKL･MNO‼PQR",
      // centered punctuation marks:
      // ⁇ U+2047, ⁈ U+2048, ⁉ U+2049, ！ U+FF01, ？ U+FF1F
      "ABC⁇DEF⁈GHI⁉JKL！MNO？PQR",
      ]) {
      const iter = seg.segment(text);
      while (!iter.following()) {
        opportunity++;
      }
    }
    breakCounts[locale + "-" + lineBreakStyle] = opportunity;
  }
}
// In Japanese
// Just test the break count in loose mode is greater than normal mode.
assertTrue(breakCounts["ja-loose"] > breakCounts["ja-normal"]);
// and test the break count in normal mode is greater than strict mode.
assertTrue(breakCounts["ja-normal"] > breakCounts["ja-strict"]);
// In Chinese
// Just test the break count in loose mode is greater than normal mode.
assertTrue(breakCounts["zh-loose"] > breakCounts["zh-normal"]);
// and test the break count in normal mode is greater than strict mode.
assertTrue(breakCounts["zh-normal"] > breakCounts["zh-strict"]);
// In English, French and Korean
assertTrue(breakCounts["en-loose"] >= breakCounts["en-normal"]);
assertTrue(breakCounts["fr-loose"] >= breakCounts["fr-normal"]);
assertTrue(breakCounts["ko-loose"] >= breakCounts["ko-normal"]);
// and test the break count in normal mode is greater than strict mode.
assertTrue(breakCounts["en-normal"] > breakCounts["en-strict"]);
assertTrue(breakCounts["fr-normal"] > breakCounts["fr-strict"]);
assertTrue(breakCounts["ko-normal"] > breakCounts["ko-strict"]);
