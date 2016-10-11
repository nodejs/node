// Flags: --expose_internals
'use strict';

const common = require('../common');
const assert = require('assert');
const readline = require('internal/readline');

if (!process.binding('config').hasIntl) {
  common.skip('missing intl... skipping test');
  return;
}

// Test column width
assert.strictEqual(readline.getStringWidth('a'), 1);
assert.strictEqual(readline.getStringWidth('丁'), 2);
assert.strictEqual(readline.getStringWidth('\ud83d\udc78\ud83c\udfff'), 2);
assert.strictEqual(readline.getStringWidth('👅'), 2);
assert.strictEqual(readline.getStringWidth('\n'), 0);
assert.strictEqual(readline.getStringWidth('\u200Ef\u200F'), 1);
assert.strictEqual(readline.getStringWidth(97), 1);

// The following is an emoji sequence. In some implementations, it is
// represented as a single glyph, in other implementations as a sequence
// of individual glyphs. By default, the algorithm will assume the single
// glyph interpretation and return a value of 2. By passing the
// expandEmojiSequence: true option, each component will be counted
// individually.
assert.strictEqual(readline.getStringWidth('👩‍👩‍👧‍👧'), 2);
assert.strictEqual(
    readline.getStringWidth('👩‍👩‍👧‍👧', {expandEmojiSequence: true}), 8);

// By default, unicode characters whose width is considered ambiguous will
// be considered half-width. For these characters, getStringWidth will return
// 1. In some contexts, however, it is more appropriate to consider them full
// width. By default, the algorithm will assume half width. By passing
// the ambiguousAsFullWidth: true option, ambiguous characters will be counted
// as 2 columns.
assert.strictEqual(readline.getStringWidth('\u01d4'), 1);
assert.strictEqual(
    readline.getStringWidth('\u01d4', {ambiguousAsFullWidth: true}), 2);

// Control chars and combining chars are zero
assert.strictEqual(readline.getStringWidth('\u200E\n\u220A\u20D2'), 1);
