// Flags: --expose_internals
'use strict';
const common = require('../common');

if (!common.hasIntl)
  common.skip('missing Intl');

const assert = require('assert');
const readline = require('internal/readline');

// Test column width

// Ll (Lowercase Letter): LATIN SMALL LETTER A
assert.strictEqual(readline.getStringWidth('a'), 1);
assert.strictEqual(readline.getStringWidth(0x0061), 1);
// Lo (Other Letter)
assert.strictEqual(readline.getStringWidth('丁'), 2);
assert.strictEqual(readline.getStringWidth(0x4E01), 2);
// Surrogate pairs
assert.strictEqual(readline.getStringWidth('\ud83d\udc78\ud83c\udfff'), 2);
assert.strictEqual(readline.getStringWidth('👅'), 2);
// Cs (Surrogate): High Surrogate
assert.strictEqual(readline.getStringWidth('\ud83d'), 1);
// Cs (Surrogate): Low Surrogate
assert.strictEqual(readline.getStringWidth('\udc78'), 1);
// Cc (Control): NULL
assert.strictEqual(readline.getStringWidth(0), 0);
// Cc (Control): BELL
assert.strictEqual(readline.getStringWidth(0x0007), 0);
// Cc (Control): LINE FEED
assert.strictEqual(readline.getStringWidth('\n'), 0);
// Cf (Format): SOFT HYPHEN
assert.strictEqual(readline.getStringWidth(0x00AD), 1);
// Cf (Format): LEFT-TO-RIGHT MARK
// Cf (Format): RIGHT-TO-LEFT MARK
assert.strictEqual(readline.getStringWidth('\u200Ef\u200F'), 1);
// Cn (Unassigned): Not a character
assert.strictEqual(readline.getStringWidth(0x10FFEF), 1);
// Cn (Unassigned): Not a character (but in a CJK range)
assert.strictEqual(readline.getStringWidth(0x3FFEF), 2);
// Mn (Nonspacing Mark): COMBINING ACUTE ACCENT
assert.strictEqual(readline.getStringWidth(0x0301), 0);
// Mc (Spacing Mark): BALINESE ADEG ADEG
// Chosen as its Canonical_Combining_Class is not 0, but is not a 0-width
// character.
assert.strictEqual(readline.getStringWidth(0x1B44), 1);
// Me (Enclosing Mark): COMBINING ENCLOSING CIRCLE
assert.strictEqual(readline.getStringWidth(0x20DD), 0);

// The following is an emoji sequence. In some implementations, it is
// represented as a single glyph, in other implementations as a sequence
// of individual glyphs. By default, the algorithm will assume the single
// glyph interpretation and return a value of 2. By passing the
// expandEmojiSequence: true option, each component will be counted
// individually.
assert.strictEqual(readline.getStringWidth('👩‍👩‍👧‍👧'), 2);
assert.strictEqual(
  readline.getStringWidth('👩‍👩‍👧‍👧', { expandEmojiSequence: true }), 8);

// By default, unicode characters whose width is considered ambiguous will
// be considered half-width. For these characters, getStringWidth will return
// 1. In some contexts, however, it is more appropriate to consider them full
// width. By default, the algorithm will assume half width. By passing
// the ambiguousAsFullWidth: true option, ambiguous characters will be counted
// as 2 columns.
assert.strictEqual(readline.getStringWidth('\u01d4'), 1);
assert.strictEqual(
  readline.getStringWidth('\u01d4', { ambiguousAsFullWidth: true }), 2);

// Control chars and combining chars are zero
assert.strictEqual(readline.getStringWidth('\u200E\n\u220A\u20D2'), 1);
