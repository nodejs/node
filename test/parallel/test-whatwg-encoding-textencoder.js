// Flags: --expose-internals
'use strict';

const common = require('../common');
const assert = require('assert');
const { TextDecoder, TextEncoder } = require('util');
const { customInspectSymbol: inspect } = require('internal/util');

const encoded = Buffer.from([0xef, 0xbb, 0xbf, 0x74, 0x65,
                             0x73, 0x74, 0xe2, 0x82, 0xac]);

// Make Sure TextEncoder exists
assert(TextEncoder);

// Test TextEncoder
const enc = new TextEncoder();
assert(enc);
const buf = enc.encode('\ufefftest€');

assert.strictEqual(Buffer.compare(buf, encoded), 0);

{
  const fn = TextEncoder.prototype[inspect];
  assert.doesNotThrow(() => {
    fn.call(new TextEncoder(), Infinity, {});
  });

  [{}, [], true, 1, '', new TextDecoder()].forEach((i) => {
    assert.throws(() => fn.call(i, Infinity, {}),
                  common.expectsError({
                    code: 'ERR_INVALID_THIS',
                    type: TypeError,
                    message: 'Value of "this" must be of type TextEncoder'
                  }));
  });
}
