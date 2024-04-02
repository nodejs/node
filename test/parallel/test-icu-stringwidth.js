// Flags: --expose-internals
'use strict';
const common = require('../common');

const assert = require('assert');
const { getStringWidth } = require('internal/util/inspect');

// Test column width

// Ll (Lowercase Letter): LATIN SMALL LETTER A
assert.strictEqual(getStringWidth('a'), 1);
assert.strictEqual(getStringWidth(String.fromCharCode(0x0061)), 1);
// Lo (Other Letter)
assert.strictEqual(getStringWidth('丁'), 2);
assert.strictEqual(getStringWidth(String.fromCharCode(0x4E01)), 2);
// Surrogate pairs
assert.strictEqual(getStringWidth('\ud83d\udc78\ud83c\udfff'), 4);
assert.strictEqual(getStringWidth('👅'), 2);
// Cs (Surrogate): High Surrogate
assert.strictEqual(getStringWidth('\ud83d'), 1);
// Cs (Surrogate): Low Surrogate
assert.strictEqual(getStringWidth('\udc78'), 1);
// Cc (Control): NULL
assert.strictEqual(getStringWidth('\u0000'), 0);
// Cc (Control): BELL
assert.strictEqual(getStringWidth(String.fromCharCode(0x0007)), 0);
// Cc (Control): LINE FEED
assert.strictEqual(getStringWidth('\n'), 0);
// Cf (Format): SOFT HYPHEN
assert.strictEqual(getStringWidth(String.fromCharCode(0x00AD)), 1);
// Cf (Format): LEFT-TO-RIGHT MARK
// Cf (Format): RIGHT-TO-LEFT MARK
assert.strictEqual(getStringWidth('\u200Ef\u200F'), 1);
// Cn (Unassigned): Not a character
assert.strictEqual(getStringWidth(String.fromCharCode(0x10FFEF)), 1);
// Cn (Unassigned): Not a character (but in a CJK range)
assert.strictEqual(getStringWidth(String.fromCharCode(0x3FFEF)), 1);
// Mn (Nonspacing Mark): COMBINING ACUTE ACCENT
assert.strictEqual(getStringWidth(String.fromCharCode(0x0301)), 0);
// Mc (Spacing Mark): BALINESE ADEG ADEG
// Chosen as its Canonical_Combining_Class is not 0, but is not a 0-width
// character.
assert.strictEqual(getStringWidth(String.fromCharCode(0x1B44)), 1);
// Me (Enclosing Mark): COMBINING ENCLOSING CIRCLE
assert.strictEqual(getStringWidth(String.fromCharCode(0x20DD)), 0);

// The following is an emoji sequence with ZWJ (zero-width-joiner). In some
// implementations, it is represented as a single glyph, in other
// implementations as a sequence of individual glyphs. By default, each
// component will be counted individually, since not a lot of systems support
// these fully.
// See https://www.unicode.org/reports/tr51/tr51-16.html#Emoji_ZWJ_Sequences
assert.strictEqual(getStringWidth('👩‍👩‍👧‍👧'), 8);
// TODO(BridgeAR): This should have a width of two and six. The heart contains
// the \uFE0F variation selector that indicates that it should be displayed as
// emoji instead of as text. Emojis are all full width characters when not being
// rendered as text.
// https://en.wikipedia.org/wiki/Variation_Selectors_(Unicode_block)
assert.strictEqual(getStringWidth('❤️'), 1);
assert.strictEqual(getStringWidth('👩‍❤️‍👩'), 5);
// The length of one is correct. It is an emoji treated as text.
assert.strictEqual(getStringWidth('❤'), 1);

// By default, unicode characters whose width is considered ambiguous will
// be considered half-width. For these characters, getStringWidth will return
// 1. In some contexts, however, it is more appropriate to consider them full
// width. By default, the algorithm will assume half width.
assert.strictEqual(getStringWidth('\u01d4'), 1);

// Control chars and combining chars are zero
assert.strictEqual(getStringWidth('\u200E\n\u220A\u20D2'), 1);

// Test that the fast path for ASCII characters yields results consistent
// with the 'slow' path.
for (let i = 0; i < 256; i++) {
  const char = String.fromCharCode(i);
  assert.strictEqual(
    getStringWidth(char + '🎉'),
    getStringWidth(char) + 2);

  if (i < 32 || (i >= 127 && i < 160)) {  // Control character
    assert.strictEqual(getStringWidth(char), 0);
  } else {  // Regular ASCII character
    assert.strictEqual(getStringWidth(char), 1);
  }
}

if (common.hasIntl) {
  const a = '한글'.normalize('NFD'); // 한글
  const b = '한글'.normalize('NFC'); // 한글
  assert.strictEqual(a.length, 6);
  assert.strictEqual(b.length, 2);
  assert.strictEqual(getStringWidth(a), 4);
  assert.strictEqual(getStringWidth(b), 4);
}
