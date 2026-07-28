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

// Test for the difference between Latin1 and Windows-1252 in the 128-159
// range
// Ref: https://github.com/nodejs/node/issues/60888
test('TextDecoder correctly decodes windows-1252 special characters in ' +
     '128-159 range', { skip: !common.hasIntl }, () => {
  const decoder = new TextDecoder('windows-1252');

  // Test specific characters that differ between Latin1 and Windows-1252.
  // € Euro sign
  assert.strictEqual(decoder.decode(Uint8Array.of(128)).codePointAt(0),
                     8364);
  // ‚ Single low-9 quotation mark
  assert.strictEqual(decoder.decode(Uint8Array.of(130)).codePointAt(0),
                     8218);
  // Latin small letter f with hook (ƒ)
  assert.strictEqual(decoder.decode(Uint8Array.of(131)).codePointAt(0),
                     402);
  // Ÿ Latin capital letter Y with diaeresis
  assert.strictEqual(decoder.decode(Uint8Array.of(159)).codePointAt(0),
                     376);

  // Test the full range to ensure no character is treated as Latin1
  // Directly.
  const expectedMappings = [
    [128, 8364], [129, 129], [130, 8218], [131, 402], [132, 8222],
    [133, 8230], [134, 8224], [135, 8225], [136, 710], [137, 8240],
    [138, 352], [139, 8249], [140, 338], [141, 141], [142, 381],
    [143, 143], [144, 144], [145, 8216], [146, 8217], [147, 8220],
    [148, 8221], [149, 8226], [150, 8211], [151, 8212], [152, 732],
    [153, 8482], [154, 353], [155, 8250], [156, 339], [157, 157],
    [158, 382], [159, 376],
  ];

  for (const [byte, expectedCodePoint] of expectedMappings) {
    const result = decoder.decode(Uint8Array.of(byte));
    const actualCodePoint = result.codePointAt(0);
    assert.strictEqual(
      actualCodePoint,
      expectedCodePoint,
      `Byte 0x${byte.toString(16)} should decode to ` +
      `U+${expectedCodePoint.toString(16)} but got ` +
      `U+${actualCodePoint.toString(16)}`
    );
  }
});
