// Flags: --expose-internals
'use strict';

const common = require('../common');

const assert = require('assert');
const { TextDecoder, TextEncoder } = require('util');
const { customInspectSymbol: inspect } = require('internal/util');

const buf = Buffer.from([0xef, 0xbb, 0xbf, 0x74, 0x65,
                         0x73, 0x74, 0xe2, 0x82, 0xac]);

// Make Sure TextDecoder exist
assert(TextDecoder);

// Test TextDecoder, UTF-8, fatal: false, ignoreBOM: false
{
  ['unicode-1-1-utf-8', 'utf8', 'utf-8'].forEach((i) => {
    const dec = new TextDecoder(i);
    assert.strictEqual(dec.encoding, 'utf-8');
    const res = dec.decode(buf);
    assert.strictEqual(res, 'test€');
  });

  ['unicode-1-1-utf-8', 'utf8', 'utf-8'].forEach((i) => {
    const dec = new TextDecoder(i);
    let res = '';
    res += dec.decode(buf.slice(0, 8), { stream: true });
    res += dec.decode(buf.slice(8));
    assert.strictEqual(res, 'test€');
  });
}

// Test TextDecoder, UTF-8, fatal: false, ignoreBOM: true
{
  ['unicode-1-1-utf-8', 'utf8', 'utf-8'].forEach((i) => {
    const dec = new TextDecoder(i, { ignoreBOM: true });
    const res = dec.decode(buf);
    assert.strictEqual(res, '\ufefftest€');
  });

  ['unicode-1-1-utf-8', 'utf8', 'utf-8'].forEach((i) => {
    const dec = new TextDecoder(i, { ignoreBOM: true });
    let res = '';
    res += dec.decode(buf.slice(0, 8), { stream: true });
    res += dec.decode(buf.slice(8));
    assert.strictEqual(res, '\ufefftest€');
  });
}

// Test TextDecoder, UTF-8, fatal: true, ignoreBOM: false
if (common.hasIntl) {
  ['unicode-1-1-utf-8', 'utf8', 'utf-8'].forEach((i) => {
    const dec = new TextDecoder(i, { fatal: true });
    common.expectsError(() => dec.decode(buf.slice(0, 8)),
                        {
                          code: 'ERR_ENCODING_INVALID_ENCODED_DATA',
                          type: TypeError,
                          message: 'The encoded data was not valid ' +
                          'for encoding utf-8'
                        });
  });

  ['unicode-1-1-utf-8', 'utf8', 'utf-8'].forEach((i) => {
    const dec = new TextDecoder(i, { fatal: true });
    assert.doesNotThrow(() => dec.decode(buf.slice(0, 8), { stream: true }));
    assert.doesNotThrow(() => dec.decode(buf.slice(8)));
  });
} else {
  common.expectsError(
    () => new TextDecoder('utf-8', { fatal: true }),
    {
      code: 'ERR_NO_ICU',
      type: TypeError,
      message: '"fatal" option is not supported on Node.js compiled without ICU'
    });
}

// Test TextDecoder, UTF-16le
{
  const dec = new TextDecoder('utf-16le');
  const res = dec.decode(Buffer.from('test€', 'utf-16le'));
  assert.strictEqual(res, 'test€');
}

// Test TextDecoder, UTF-16be
if (common.hasIntl) {
  const dec = new TextDecoder('utf-16be');
  const res = dec.decode(Buffer.from('test€', 'utf-16le').swap16());
  assert.strictEqual(res, 'test€');
}

{
  const inspectFn = TextDecoder.prototype[inspect];
  const decodeFn = TextDecoder.prototype.decode;
  const {
    encoding: { get: encodingGetter },
    fatal: { get: fatalGetter },
    ignoreBOM: { get: ignoreBOMGetter },
  } = Object.getOwnPropertyDescriptors(TextDecoder.prototype);

  const instance = new TextDecoder();

  const expectedError = {
    code: 'ERR_INVALID_THIS',
    type: TypeError,
    message: 'Value of "this" must be of type TextDecoder'
  };

  assert.doesNotThrow(() => inspectFn.call(instance, Infinity, {}));
  assert.doesNotThrow(() => decodeFn.call(instance));
  assert.doesNotThrow(() => encodingGetter.call(instance));
  assert.doesNotThrow(() => fatalGetter.call(instance));
  assert.doesNotThrow(() => ignoreBOMGetter.call(instance));

  const invalidThisArgs = [{}, [], true, 1, '', new TextEncoder()];
  invalidThisArgs.forEach((i) => {
    common.expectsError(() => inspectFn.call(i, Infinity, {}), expectedError);
    common.expectsError(() => decodeFn.call(i), expectedError);
    common.expectsError(() => encodingGetter.call(i), expectedError);
    common.expectsError(() => fatalGetter.call(i), expectedError);
    common.expectsError(() => ignoreBOMGetter.call(i), expectedError);
  });
}

// From: https://github.com/w3c/web-platform-tests/blob/master/encoding/api-basics.html
function testDecodeSample(encoding, string, bytes) {
  assert.strictEqual(
    new TextDecoder(encoding).decode(new Uint8Array(bytes)),
    string);
  assert.strictEqual(
    new TextDecoder(encoding).decode(new Uint8Array(bytes).buffer),
    string);
}

// z (ASCII U+007A), cent (Latin-1 U+00A2), CJK water (BMP U+6C34),
// G-Clef (non-BMP U+1D11E), PUA (BMP U+F8FF), PUA (non-BMP U+10FFFD)
// byte-swapped BOM (non-character U+FFFE)
const sample = 'z\xA2\u6C34\uD834\uDD1E\uF8FF\uDBFF\uDFFD\uFFFE';

{
  const encoding = 'utf-8';
  const string = sample;
  const bytes = [
    0x7A, 0xC2, 0xA2, 0xE6, 0xB0, 0xB4,
    0xF0, 0x9D, 0x84, 0x9E, 0xEF, 0xA3,
    0xBF, 0xF4, 0x8F, 0xBF, 0xBD, 0xEF,
    0xBF, 0xBE
  ];
  const encoded = new TextEncoder().encode(string);
  assert.deepStrictEqual([].slice.call(encoded), bytes);
  assert.strictEqual(
    new TextDecoder(encoding).decode(new Uint8Array(bytes)),
    string);
  assert.strictEqual(
    new TextDecoder(encoding).decode(new Uint8Array(bytes).buffer),
    string);
}

testDecodeSample(
  'utf-16le',
  sample,
  [
    0x7A, 0x00, 0xA2, 0x00, 0x34, 0x6C,
    0x34, 0xD8, 0x1E, 0xDD, 0xFF, 0xF8,
    0xFF, 0xDB, 0xFD, 0xDF, 0xFE, 0xFF
  ]
);

if (common.hasIntl) {
  testDecodeSample(
    'utf-16be',
    sample,
    [
      0x00, 0x7A, 0x00, 0xA2, 0x6C, 0x34,
      0xD8, 0x34, 0xDD, 0x1E, 0xF8, 0xFF,
      0xDB, 0xFF, 0xDF, 0xFD, 0xFF, 0xFE
    ]
  );
}

testDecodeSample(
  'utf-16',
  sample,
  [
    0x7A, 0x00, 0xA2, 0x00, 0x34, 0x6C,
    0x34, 0xD8, 0x1E, 0xDD, 0xFF, 0xF8,
    0xFF, 0xDB, 0xFD, 0xDF, 0xFE, 0xFF
  ]
);

// From: https://github.com/w3c/web-platform-tests/blob/master/encoding/api-invalid-label.html
[
  'utf-8',
  'unicode-1-1-utf-8',
  'utf8',
  'utf-16be',
  'utf-16le',
  'utf-16'
].forEach((i) => {
  ['\u0000', '\u000b', '\u00a0', '\u2028', '\u2029'].forEach((ws) => {
    common.expectsError(
      () => new TextDecoder(`${ws}${i}`),
      {
        code: 'ERR_ENCODING_NOT_SUPPORTED',
        type: RangeError
      }
    );

    common.expectsError(
      () => new TextDecoder(`${i}${ws}`),
      {
        code: 'ERR_ENCODING_NOT_SUPPORTED',
        type: RangeError
      }
    );

    common.expectsError(
      () => new TextDecoder(`${ws}${i}${ws}`),
      {
        code: 'ERR_ENCODING_NOT_SUPPORTED',
        type: RangeError
      }
    );
  });
});
