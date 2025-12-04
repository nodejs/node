'use strict';

// Tests for Windows-1252 encoding, specifically the 0x80-0x9F range
// where it differs from ISO-8859-1 (Latin-1).
// Refs: https://github.com/nodejs/node/issues/56542
// Refs: https://encoding.spec.whatwg.org/#windows-1252

require('../common');

const assert = require('assert');

// Test specific case from issue #56542
{
  const decoder = new TextDecoder('windows-1252');
  const decoded = decoder.decode(new Uint8Array([0x92]));
  assert.strictEqual(
    decoded.charCodeAt(0),
    0x2019,
    'Byte 0x92 should decode to U+2019 (') not U+0092'
  );
  assert.strictEqual(decoded, '\u2019', 'Expected right single quotation mark');
}

// Test all 32 characters in the 0x80-0x9F range where Windows-1252
// differs from ISO-8859-1. These mappings are defined by the WHATWG
// Encoding Standard.
// Source: https://encoding.spec.whatwg.org/#index-windows-1252
{
  const testCases = [
    [0x80, 0x20AC, '€'],  // EURO SIGN
    [0x81, 0x0081, '\u0081'],  // Undefined (maps to itself)
    [0x82, 0x201A, '‚'],  // SINGLE LOW-9 QUOTATION MARK
    [0x83, 0x0192, 'ƒ'],  // LATIN SMALL LETTER F WITH HOOK
    [0x84, 0x201E, '„'],  // DOUBLE LOW-9 QUOTATION MARK
    [0x85, 0x2026, '…'],  // HORIZONTAL ELLIPSIS
    [0x86, 0x2020, '†'],  // DAGGER
    [0x87, 0x2021, '‡'],  // DOUBLE DAGGER
    [0x88, 0x02C6, 'ˆ'],  // MODIFIER LETTER CIRCUMFLEX ACCENT
    [0x89, 0x2030, '‰'],  // PER MILLE SIGN
    [0x8A, 0x0160, 'Š'],  // LATIN CAPITAL LETTER S WITH CARON
    [0x8B, 0x2039, '‹'],  // SINGLE LEFT-POINTING ANGLE QUOTATION MARK
    [0x8C, 0x0152, 'Œ'],  // LATIN CAPITAL LIGATURE OE
    [0x8D, 0x008D, '\u008D'],  // Undefined (maps to itself)
    [0x8E, 0x017D, 'Ž'],  // LATIN CAPITAL LETTER Z WITH CARON
    [0x8F, 0x008F, '\u008F'],  // Undefined (maps to itself)
    [0x90, 0x0090, '\u0090'],  // Undefined (maps to itself)
    [0x91, 0x2018, '''],  // LEFT SINGLE QUOTATION MARK
    [0x92, 0x2019, '''],  // RIGHT SINGLE QUOTATION MARK
    [0x93, 0x201C, '"'],  // LEFT DOUBLE QUOTATION MARK
    [0x94, 0x201D, '"'],  // RIGHT DOUBLE QUOTATION MARK
    [0x95, 0x2022, '•'],  // BULLET
    [0x96, 0x2013, '–'],  // EN DASH
    [0x97, 0x2014, '—'],  // EM DASH
    [0x98, 0x02DC, '˜'],  // SMALL TILDE
    [0x99, 0x2122, '™'],  // TRADE MARK SIGN
    [0x9A, 0x0161, 'š'],  // LATIN SMALL LETTER S WITH CARON
    [0x9B, 0x203A, '›'],  // SINGLE RIGHT-POINTING ANGLE QUOTATION MARK
    [0x9C, 0x0153, 'œ'],  // LATIN SMALL LIGATURE OE
    [0x9D, 0x009D, '\u009D'],  // Undefined (maps to itself)
    [0x9E, 0x017E, 'ž'],  // LATIN SMALL LETTER Z WITH CARON
    [0x9F, 0x0178, 'Ÿ'],  // LATIN CAPITAL LETTER Y WITH DIAERESIS
  ];

  const decoder = new TextDecoder('windows-1252');

  for (const [byte, expectedCodePoint, expectedChar] of testCases) {
    const decoded = decoder.decode(new Uint8Array([byte]));
    const actualCodePoint = decoded.charCodeAt(0);

    assert.strictEqual(
      actualCodePoint,
      expectedCodePoint,
      `Byte 0x${byte.toString(16).toUpperCase()} should decode to ` +
      `U+${expectedCodePoint.toString(16).toUpperCase().padStart(4, '0')} ` +
      `but got U+${actualCodePoint.toString(16).toUpperCase().padStart(4, '0')}`
    );

    assert.strictEqual(
      decoded,
      expectedChar,
      `Byte 0x${byte.toString(16).toUpperCase()} should decode to ` +
      `${expectedChar} but got ${decoded}`
    );
  }
}
