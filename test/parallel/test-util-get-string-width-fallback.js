// Flags: --expose-internals
'use strict';

require('../common');
const assert = require('assert');
const { getStringDisplayWidthFallback } = require('internal/util/inspect');

// The width used by util.getStringWidth() when Node.js is built without ICU.
const cases = [
  ['', 0],
  ['hello', 5],
  ['a你b', 4],
  ['Ｆ', 2],
  ['ｶ\uFF9E', 2],
  ['\u{20000}', 2],
  ['\u0007', 0],
  ['あ\tい', 4],
  ['e\u0301', 1],
  ['\u200B', 0],
  ['\uD83D', 0],
  ['\u001B[31mred\u001B[39m', 3],
  ['👩\u200D👩\u200D👧\u200D👧', 2],
  ['a👩\u200D👩\u200D👧b', 4],
  ['👨\u200D💻', 2],
  ['🏳\uFE0F\u200D🌈', 2],
  ['❤\uFE0F\u200D🔥', 2],
  ['👩\u200D❤\uFE0F\u200D👨', 2],
  ['\u26F9\uFE0F\u200D\u2642\uFE0F', 2],
  ['👍🏽', 2],
  ['🇮🇹', 2],
  ['🤗🚀', 4],
  ['❤\uFE0F', 2],
  ['❤', 1],
  ['\u00A9\uFE0F', 2],
  ['a\uFE0F', 1],
  ['1\uFE0F\u20E3', 2],
  ['1\u20E3', 2],
  ['#\u20E3', 2],
  ['a\u200Db', 2],
  ['你\u200D你', 4],
  ['\u1100\u1161', 2],
  ['\uAC00\u1161', 2],  // Syllable and a vowel jamo, not composed by NFC.
  ['\uAC01\u11A8', 2],  // Syllable and a trailing jamo.
  ['क\u093F', 2],
];

for (const [str, width] of cases) {
  assert.strictEqual(getStringDisplayWidthFallback(str), width, `width of ${JSON.stringify(str)}`);
}
