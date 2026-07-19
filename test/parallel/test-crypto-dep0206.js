'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const crypto = require('crypto');

common.expectWarning({
  DeprecationWarning: {
    DEP0206: 'Calling Hmac.digest() more than once is deprecated.',
  },
});

// Verify runtime deprecation warning for calling digest() more than once.
{
  const h = crypto.createHmac('sha1', 'key').update('data');
  h.digest('hex');
  h.digest('hex');
}

// Check initialized -> uninitialized state transition after calling digest().
{
  const expected =
      '\u0010\u0041\u0052\u00c5\u00bf\u00dc\u00a0\u007b\u00c6\u0033' +
      '\u00ee\u00bd\u0046\u0019\u009f\u0002\u0055\u00c9\u00f4\u009d';
  {
    const h = crypto.createHmac('sha1', 'key').update('data');
    assert.deepStrictEqual(h.digest('buffer'), Buffer.from(expected, 'latin1'));
    assert.deepStrictEqual(h.digest('buffer'), Buffer.from(''));
  }
  {
    const h = crypto.createHmac('sha1', 'key').update('data');
    assert.strictEqual(h.digest('latin1'), expected);
    assert.strictEqual(h.digest('latin1'), '');
  }
}

// Check initialized -> uninitialized state transition after calling digest().
// Calls to update() omitted intentionally.
{
  const expected =
      '\u00f4\u002b\u00b0\u00ee\u00b0\u0018\u00eb\u00bd\u0045\u0097' +
      '\u00ae\u0072\u0013\u0071\u001e\u00c6\u0007\u0060\u0084\u003f';
  {
    const h = crypto.createHmac('sha1', 'key');
    assert.deepStrictEqual(h.digest('buffer'), Buffer.from(expected, 'latin1'));
    assert.deepStrictEqual(h.digest('buffer'), Buffer.from(''));
  }
  {
    const h = crypto.createHmac('sha1', 'key');
    assert.strictEqual(h.digest('latin1'), expected);
    assert.strictEqual(h.digest('latin1'), '');
  }
}
