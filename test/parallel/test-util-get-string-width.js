'use strict';

const common = require('../common');
const assert = require('assert');
const util = require('util');

const { getStringWidth } = util;

assert.strictEqual(getStringWidth(''), 0);
assert.strictEqual(getStringWidth('a'), 1);
assert.strictEqual(getStringWidth('hello'), 5);
assert.strictEqual(getStringWidth('a你b'), 4);

// Full-width characters count as two columns.
assert.strictEqual(getStringWidth('丁'), 2);
assert.strictEqual(getStringWidth('你好'), 4);
assert.strictEqual(getStringWidth('가'), 2);
assert.strictEqual(getStringWidth('👅'), 2);
assert.strictEqual(getStringWidth('Ｆ'), 2);  // Fullwidth Latin letter.
assert.strictEqual(getStringWidth('ｱ'), 1);  // Halfwidth katakana.
assert.strictEqual(getStringWidth('ｶ\uFF9E'), 2);  // Halfwidth dakuten.
assert.strictEqual(getStringWidth('\u{1D49C}'), 1);  // Astral, not wide.
assert.strictEqual(getStringWidth('\u{20000}'), 2);  // CJK Extension B.
assert.strictEqual(getStringWidth('±'), 1);  // Ambiguous width is narrow.

// Zero-width characters count as zero.
assert.strictEqual(getStringWidth('\u0000'), 0);
assert.strictEqual(getStringWidth('\u0007'), 0);
assert.strictEqual(getStringWidth('\n'), 0);
assert.strictEqual(getStringWidth('\r\n'), 0);
assert.strictEqual(getStringWidth('\t'), 0);
assert.strictEqual(getStringWidth('あ\tい'), 4);
assert.strictEqual(getStringWidth('\u0301'), 0);  // Combining acute accent.
assert.strictEqual(getStringWidth('e\u0301'), 1);
assert.strictEqual(getStringWidth('\u200B'), 0);  // Zero-width space.
assert.strictEqual(getStringWidth('a\u200Fb'), 2);  // Right-to-left mark.
assert.strictEqual(getStringWidth('\uD83D'), 0);  // Lone surrogate.

// ANSI escape sequences are ignored.
assert.strictEqual(getStringWidth('\u001B[31mred\u001B[39m'), 3);
assert.strictEqual(getStringWidth(util.styleText('bold', 'hi')), 2);
assert.strictEqual(getStringWidth(util.styleText(['bold', 'red'], 'ok')), 2);
assert.strictEqual(
  getStringWidth('\u001B]8;;https://nodejs.org\u0007text\u001B]8;;\u0007'), 4);

// An emoji sequence is rendered as one glyph of two columns.
assert.strictEqual(getStringWidth('👩\u200D👩\u200D👧\u200D👧'), 2);
assert.strictEqual(getStringWidth('a👩\u200D👩\u200D👧b'), 4);
assert.strictEqual(getStringWidth('👨\u200D💻'), 2);
assert.strictEqual(getStringWidth('🏳\uFE0F\u200D🌈'), 2);
assert.strictEqual(getStringWidth('👍🏽'), 2);  // Skin tone modifier.
assert.strictEqual(getStringWidth('🇮🇹'), 2);  // Flag.
assert.strictEqual(getStringWidth('❤\uFE0F'), 2);  // Emoji presentation.
assert.strictEqual(getStringWidth('❤'), 1);  // Text presentation.
assert.strictEqual(getStringWidth('1\uFE0F\u20E3'), 2);  // Keycap.
assert.strictEqual(getStringWidth('1\u20E3'), 2);  // Keycap without selector.
assert.strictEqual(getStringWidth('#\u20E3'), 2);
assert.strictEqual(getStringWidth('\u00A9\uFE0F'), 2);  // Copyright sign.
assert.strictEqual(getStringWidth('a\u200Db'), 2);  // Joiner between letters.
assert.strictEqual(getStringWidth('\u{1F917}\u{1F680}'), 4);
assert.strictEqual(getStringWidth('\u26F9\uFE0F\u200D\u2642\uFE0F'), 2);  // Gender sequence.
assert.strictEqual(getStringWidth('😀😀'), 4);

// Hangul jamo form syllable blocks, spacing marks take their own column.
assert.strictEqual(getStringWidth('\u1100\u1161'), 2);
assert.strictEqual(getStringWidth('\u1100\u1161\u11A8'), 2);
assert.strictEqual(getStringWidth('क\u093F'), 2);  // Devanagari vowel sign.

// Grapheme clusters need the character break rules, which small-icu lacks.
if (common.hasFullICU) {
  assert.strictEqual(getStringWidth('\u00A0'), 1);  // No-break space.
  assert.strictEqual(getStringWidth('ก\u0E34'), 1);  // Thai vowel mark.
  assert.strictEqual(getStringWidth('\u00AD'), 0);  // Soft hyphen.
  assert.strictEqual(getStringWidth('⌚'), 2);  // Emoji presentation by default.
  assert.strictEqual(getStringWidth('\u1100\u1100\u1161'), 4);  // Unmatched jamo.
  assert.strictEqual(getStringWidth('\u1100\u1161가'), 4);  // Jamo and a syllable.
  assert.strictEqual(getStringWidth('\u1160'), 0);  // Jungseong filler.
  assert.strictEqual(getStringWidth('\u1161'), 1);  // Lone vowel jamo.
  assert.strictEqual(getStringWidth('🧑🏽\u200D🤝\u200D🧑🏻'), 2);
  assert.strictEqual(getStringWidth('\u{E0001}'), 0);  // Language tag.
  assert.strictEqual(getStringWidth('\u26F9\u200D\u2642'), 2);  // Unqualified sequence.
  assert.strictEqual(getStringWidth('*\u20E3'), 2);
  assert.strictEqual(getStringWidth('a\u20E3'), 1);  // Not a keycap base.
  assert.strictEqual(getStringWidth('\u1100\u0301'), 2);
  assert.strictEqual(getStringWidth('\u1100\u093F'), 3);  // Jamo with a spacing mark.
  assert.strictEqual(getStringWidth('\u{1F1E6}\u{1F1E6}'), 1);  // Not a flag.
  assert.strictEqual(getStringWidth('1\u0301'), 1);  // Not a keycap.
  assert.strictEqual(getStringWidth('!\u20E3'), 1);
  assert.strictEqual(getStringWidth('\u0D4E\u0D15'), 1);  // Malayalam dot reph, a prepended letter.
}

for (const value of [undefined, null, 1, true, {}, [], Symbol('s')]) {
  assert.throws(() => getStringWidth(value), {
    code: 'ERR_INVALID_ARG_TYPE',
    name: 'TypeError',
  });
}
