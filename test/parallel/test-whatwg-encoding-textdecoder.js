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
    assert.throws(() => dec.decode(buf.slice(0, 8)),
                  common.expectsError({
                    code: 'ERR_ENCODING_INVALID_ENCODED_DATA',
                    type: TypeError,
                    message: 'The encoded data was not valid for encoding utf-8'
                  }));
  });

  ['unicode-1-1-utf-8', 'utf8', 'utf-8'].forEach((i) => {
    const dec = new TextDecoder(i, { fatal: true });
    assert.doesNotThrow(() => dec.decode(buf.slice(0, 8), { stream: true }));
    assert.doesNotThrow(() => dec.decode(buf.slice(8)));
  });
} else {
  assert.throws(
    () => new TextDecoder('utf-8', { fatal: true }),
    common.expectsError({
      code: 'ERR_NO_ICU',
      type: TypeError,
      message: '"fatal" option is not supported on Node.js compiled without ICU'
    }));
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
  const fn = TextDecoder.prototype[inspect];
  assert.doesNotThrow(() => {
    fn.call(new TextDecoder(), Infinity, {});
  });

  [{}, [], true, 1, '', new TextEncoder()].forEach((i) => {
    assert.throws(() => fn.call(i, Infinity, {}),
                  common.expectsError({
                    code: 'ERR_INVALID_THIS',
                    type: TypeError,
                    message: 'Value of "this" must be of type TextDecoder'
                  }));
  });
}
