// Flags: --expose-internals

// This tests Node.js-specific behaviors of TextDecoder

'use strict';

const common = require('../common');

const assert = require('assert');
const { customInspectSymbol: inspect } = require('internal/util');
const util = require('util');

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
    dec.decode(buf.slice(0, 8), { stream: true });
    dec.decode(buf.slice(8));
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

// Test TextDecoder, label undefined, options null
{
  const dec = new TextDecoder(undefined, null);
  assert.strictEqual(dec.encoding, 'utf-8');
  assert.strictEqual(dec.fatal, false);
  assert.strictEqual(dec.ignoreBOM, false);
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

// Test TextDecoder inspect with hidden fields
{
  const dec = new TextDecoder('utf-8', { ignoreBOM: true });
  if (common.hasIntl) {
    assert.strictEqual(
      util.inspect(dec, { showHidden: true }),
      'TextDecoder {\n  encoding: \'utf-8\',\n  fatal: false,\n  ' +
      'ignoreBOM: true,\n  [Symbol(flags)]: 4,\n  [Symbol(handle)]: {} }'
    );
  } else {
    assert.strictEqual(
      util.inspect(dec, { showHidden: true }),
      'TextDecoder {\n  encoding: \'utf-8\',\n  fatal: false,\n  ' +
      'ignoreBOM: true,\n  [Symbol(flags)]: 4,\n  [Symbol(handle)]:\n   ' +
      'StringDecoder {\n     encoding: \'utf8\',\n     ' +
      '[Symbol(kNativeDecoder)]: <Buffer 00 00 00 00 00 00 01> } }'
    );
  }
}


// Test TextDecoder inspect without hidden fields
{
  const dec = new TextDecoder('utf-8', { ignoreBOM: true });
  assert.strictEqual(
    util.inspect(dec, { showHidden: false }),
    'TextDecoder { encoding: \'utf-8\', fatal: false, ignoreBOM: true }'
  );
}

// Test TextDecoder inspect with negative depth
{
  const dec = new TextDecoder();
  assert.strictEqual(util.inspect(dec, { depth: -1 }), '[Object]');
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

  inspectFn.call(instance, Infinity, {});
  decodeFn.call(instance);
  encodingGetter.call(instance);
  fatalGetter.call(instance);
  ignoreBOMGetter.call(instance);

  const invalidThisArgs = [{}, [], true, 1, '', new TextEncoder()];
  invalidThisArgs.forEach((i) => {
    common.expectsError(() => inspectFn.call(i, Infinity, {}), expectedError);
    common.expectsError(() => decodeFn.call(i), expectedError);
    common.expectsError(() => encodingGetter.call(i), expectedError);
    common.expectsError(() => fatalGetter.call(i), expectedError);
    common.expectsError(() => ignoreBOMGetter.call(i), expectedError);
  });
}

{
  common.expectsError(
    () => new TextDecoder('utf-8', 1),
    {
      code: 'ERR_INVALID_ARG_TYPE',
      type: TypeError
    }
  );
}
