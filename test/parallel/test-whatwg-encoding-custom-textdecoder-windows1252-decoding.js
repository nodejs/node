'use strict';

const common = require('../common');

if (!common.hasIntl)
  common.skip('missing Intl');

const assert = require('assert');

{
  // Windows-1252 mapping for bytes 0x7F-0x9F as [byte, unicode code point]
  const win1252_0x7F_0x9F = [
    [0x7F, 0x007F], // DELETE
    [0x80, 0x20AC], // EURO SIGN
    [0x81, 0x0081], // UNDEFINED
    [0x82, 0x201A], // SINGLE LOW-9 QUOTATION MARK
    [0x83, 0x0192], // LATIN SMALL LETTER F WITH HOOK
    [0x84, 0x201E], // DOUBLE LOW-9 QUOTATION MARK
    [0x85, 0x2026], // HORIZONTAL ELLIPSIS
    [0x86, 0x2020], // DAGGER
    [0x87, 0x2021], // DOUBLE DAGGER
    [0x88, 0x02C6], // CIRCUMFLEX
    [0x89, 0x2030], // PER MILLE SIGN
    [0x8A, 0x0160], // LATIN CAPITAL LETTER S WITH CARON
    [0x8B, 0x2039], // SINGLE LEFT-POINTING ANGLE QUOTATION MARK
    [0x8C, 0x0152], // LATIN CAPITAL LIGATURE OE
    [0x8D, 0x008D], // UNDEFINED
    [0x8E, 0x017D], // LATIN CAPITAL LETTER Z WITH CARON
    [0x8F, 0x008F], // UNDEFINED
    [0x90, 0x0090], // UNDEFINED
    [0x91, 0x2018], // LEFT SINGLE QUOTATION MARK
    [0x92, 0x2019], // RIGHT SINGLE QUOTATION MARK
    [0x93, 0x201C], // LEFT DOUBLE QUOTATION MARK
    [0x94, 0x201D], // RIGHT DOUBLE QUOTATION MARK
    [0x95, 0x2022], // BULLET
    [0x96, 0x2013], // EN DASH
    [0x97, 0x2014], // EM DASH
    [0x98, 0x02DC], // SMALL TILDE
    [0x99, 0x2122], // TRADE MARK SIGN
    [0x9A, 0x0161], // LATIN SMALL LETTER S WITH CARON
    [0x9B, 0x203A], // SINGLE RIGHT-POINTING ANGLE QUOTATION MARK
    [0x9C, 0x0153], // LATIN SMALL LIGATURE OE
    [0x9D, 0x009D], // UNDEFINED
    [0x9E, 0x017E], // LATIN SMALL LETTER Z WITH CARON
    [0x9F, 0x0178],  // LATIN CAPITAL LETTER Y WITH DIAERESIS
  ];

  for (let i = 0; i < win1252_0x7F_0x9F.length; i++) {
    const byte = win1252_0x7F_0x9F[i][0];
    const expectedUnicodeCode = win1252_0x7F_0x9F[i][1];
    const arr = new Uint8Array([byte]);
    const decoded = new TextDecoder('windows-1252').decode(arr);
    assert.strictEqual(decoded.length, 1, `Decoded string for ${byte} should have length 1`);
    const actualUnicodeCode = decoded.codePointAt(0);
    assert.strictEqual(actualUnicodeCode, expectedUnicodeCode, `Decoded code point for ${byte} should be U+${expectedUnicodeCode.toString(16).toUpperCase()}`);
  }
}
