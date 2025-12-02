'use strict';

const common = require('../common');

const test = require('node:test');
const assert = require('node:assert');

test('TextDecoder correctly decodes windows-1252 encoded data', { skip: !common.hasIntl }, () => {
  const latin1Bytes = new Uint8Array([0xc1, 0xe9, 0xf3]);

  const expectedString = 'Áéó';

  const decoder = new TextDecoder('windows-1252');
  const decodedString = decoder.decode(latin1Bytes);

  assert.strictEqual(decodedString, expectedString);
});

test('TextDecoder correctly decodes windows-1252 special characters (0x80-0x9F)', { skip: !common.hasIntl }, () => {
  const decoder = new TextDecoder('windows-1252');
  
  // Test byte 0x92 (right single quotation mark) - the main bug from issue #56542
  assert.strictEqual(decoder.decode(new Uint8Array([0x92])), '\u2019');
  assert.strictEqual(decoder.decode(new Uint8Array([0x92])).charCodeAt(0), 0x2019);
  
  // Test all special Windows-1252 characters (0x80-0x9F)
  // Excluding undefined bytes: 0x81, 0x8D, 0x8F, 0x90, 0x9D
  const testCases = [
    { byte: 0x80, expected: '\u20AC', name: 'Euro sign' },
    { byte: 0x82, expected: '\u201A', name: 'Single low-9 quotation mark' },
    { byte: 0x83, expected: '\u0192', name: 'Latin small letter f with hook' },
    { byte: 0x84, expected: '\u201E', name: 'Double low-9 quotation mark' },
    { byte: 0x85, expected: '\u2026', name: 'Horizontal ellipsis' },
    { byte: 0x86, expected: '\u2020', name: 'Dagger' },
    { byte: 0x87, expected: '\u2021', name: 'Double dagger' },
    { byte: 0x88, expected: '\u02C6', name: 'Modifier letter circumflex accent' },
    { byte: 0x89, expected: '\u2030', name: 'Per mille sign' },
    { byte: 0x8A, expected: '\u0160', name: 'Latin capital letter S with caron' },
    { byte: 0x8B, expected: '\u2039', name: 'Single left-pointing angle quotation mark' },
    { byte: 0x8C, expected: '\u0152', name: 'Latin capital ligature OE' },
    { byte: 0x8E, expected: '\u017D', name: 'Latin capital letter Z with caron' },
    { byte: 0x91, expected: '\u2018', name: 'Left single quotation mark' },
    { byte: 0x92, expected: '\u2019', name: 'Right single quotation mark' },
    { byte: 0x93, expected: '\u201C', name: 'Left double quotation mark' },
    { byte: 0x94, expected: '\u201D', name: 'Right double quotation mark' },
    { byte: 0x95, expected: '\u2022', name: 'Bullet' },
    { byte: 0x96, expected: '\u2013', name: 'En dash' },
    { byte: 0x97, expected: '\u2014', name: 'Em dash' },
    { byte: 0x98, expected: '\u02DC', name: 'Small tilde' },
    { byte: 0x99, expected: '\u2122', name: 'Trade mark sign' },
    { byte: 0x9A, expected: '\u0161', name: 'Latin small letter s with caron' },
    { byte: 0x9B, expected: '\u203A', name: 'Single right-pointing angle quotation mark' },
    { byte: 0x9C, expected: '\u0153', name: 'Latin small ligature oe' },
    { byte: 0x9E, expected: '\u017E', name: 'Latin small letter z with caron' },
    { byte: 0x9F, expected: '\u0178', name: 'Latin capital letter Y with diaeresis' },
  ];

  for (const { byte, expected, name } of testCases) {
    const result = decoder.decode(new Uint8Array([byte]));
    assert.strictEqual(result, expected, `Failed for ${name} (0x${byte.toString(16)})`);
  }
});

test('TextDecoder windows-1252 handles undefined bytes correctly', { skip: !common.hasIntl }, () => {
  const decoder = new TextDecoder('windows-1252');
  
  // Bytes 0x81, 0x8D, 0x8F, 0x90, 0x9D are undefined in Windows-1252
  // They should be passed through as their Unicode equivalents
  const undefinedBytes = [0x81, 0x8D, 0x8F, 0x90, 0x9D];
  
  for (const byte of undefinedBytes) {
    const result = decoder.decode(new Uint8Array([byte]));
    assert.strictEqual(result.charCodeAt(0), byte, 
      `Undefined byte 0x${byte.toString(16)} should map to U+00${byte.toString(16).toUpperCase()}`);
  }
});

test('TextDecoder windows-1252 handles ASCII range correctly', { skip: !common.hasIntl }, () => {
  const decoder = new TextDecoder('windows-1252');
  
  // Test ASCII range (0x00-0x7F) - should be identical to UTF-8
  const asciiBytes = new Uint8Array([0x48, 0x65, 0x6C, 0x6C, 0x6F]); // "Hello"
  assert.strictEqual(decoder.decode(asciiBytes), 'Hello');
});

test('TextDecoder windows-1252 handles Latin-1 range correctly', { skip: !common.hasIntl }, () => {
  const decoder = new TextDecoder('windows-1252');
  
  // Test Latin-1 range (0xA0-0xFF) - should be identical to ISO-8859-1
  const latin1Bytes = new Uint8Array([0xA0, 0xC0, 0xE0, 0xFF]);
  const expected = '\u00A0\u00C0\u00E0\u00FF';
  assert.strictEqual(decoder.decode(latin1Bytes), expected);
});

test('TextDecoder windows-1252 handles mixed content', { skip: !common.hasIntl }, () => {
  const decoder = new TextDecoder('windows-1252');
  
  // Mix of ASCII, Windows-1252 special chars, and Latin-1
  // "It's €100" where 's is 0x92 (right single quote) and € is 0x80
  const mixedBytes = new Uint8Array([0x49, 0x74, 0x92, 0x73, 0x20, 0x80, 0x31, 0x30, 0x30]);
  assert.strictEqual(decoder.decode(mixedBytes), 'It\u2019s \u20AC100');
});
