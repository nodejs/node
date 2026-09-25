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

const key = '0123456789abcdef';

// Verify runtime deprecation warning for calling digest() more than once.
{
  const h = crypto.createHmac('sha1', key).update('data');
  h.digest('hex');
  h.digest('hex');
}

// Check initialized -> uninitialized state transition after calling digest().
{
  const expected =
    Buffer.from('91768485754a2ca0c93be78a6cfe02a37af32ba3', 'hex');
  {
    const h = crypto.createHmac('sha1', key).update('data');
    assert.deepStrictEqual(h.digest('buffer'), expected);
    assert.deepStrictEqual(h.digest('buffer'), Buffer.from(''));
  }
  {
    const h = crypto.createHmac('sha1', key).update('data');
    assert.strictEqual(h.digest('latin1'), expected.toString('latin1'));
    assert.strictEqual(h.digest('latin1'), '');
  }
}

// Check initialized -> uninitialized state transition after calling digest().
// Calls to update() omitted intentionally.
{
  const expected =
    Buffer.from('804df868948c143aee0946c72d272ea557eaafcc', 'hex');
  {
    const h = crypto.createHmac('sha1', key);
    assert.deepStrictEqual(h.digest('buffer'), expected);
    assert.deepStrictEqual(h.digest('buffer'), Buffer.from(''));
  }
  {
    const h = crypto.createHmac('sha1', key);
    assert.strictEqual(h.digest('latin1'), expected.toString('latin1'));
    assert.strictEqual(h.digest('latin1'), '');
  }
}
