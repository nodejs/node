'use strict';
// This tests crypto.hash() works.
const common = require('../common');

if (!common.hasCrypto) common.skip('missing crypto');

const assert = require('assert');
const crypto = require('crypto');

// Test XOF hash functions and the outputLength option.
{
  // Default outputLengths.
  assert.strictEqual(
    crypto.hash('shake128', ''),
    '7f9c2ba4e88f827d616045507605853e'
  );

  assert.strictEqual(
    crypto.hash('shake256', ''),
    '46b9dd2b0ba88d13233b3feb743eeb243fcd52ea62b81b82b50c27646ed5762f'
  );

  // outputEncoding as an option.
  assert.strictEqual(
    crypto.hash('shake128', '', { outputEncoding: 'base64url' }),
    'f5wrpOiPgn1hYEVQdgWFPg'
  );

  assert.strictEqual(
    crypto.hash('shake256', '', { outputEncoding: 'base64url' }),
    'RrndKwuojRMjOz_rdD7rJD_NUupiuBuCtQwnZG7Vdi8'
  );

  assert.deepStrictEqual(
    crypto.hash('shake128', '', { outputEncoding: 'buffer' }),
    Buffer.from('f5wrpOiPgn1hYEVQdgWFPg', 'base64url')
  );

  assert.deepStrictEqual(
    crypto.hash('shake256', '', { outputEncoding: 'buffer' }),
    Buffer.from('RrndKwuojRMjOz_rdD7rJD_NUupiuBuCtQwnZG7Vdi8', 'base64url')
  );

  // Short outputLengths.
  assert.strictEqual(crypto.hash('shake128', '', { outputLength: 0 }), '');
  assert.deepStrictEqual(crypto.hash('shake128', '', { outputEncoding: 'buffer', outputLength: 0 }),
                         Buffer.alloc(0));

  assert.strictEqual(
    crypto.hash('shake128', '', { outputLength: 5 }),
    crypto.createHash('shake128', { outputLength: 5 }).update('').digest('hex')
  );
  // Check length
  assert.strictEqual(
    crypto.hash('shake128', '', { outputLength: 5 }).length,
    crypto.createHash('shake128', { outputLength: 5 }).update('').digest('hex')
      .length
  );

  assert.strictEqual(
    crypto.hash('shake128', '', { outputLength: 15 }),
    crypto.createHash('shake128', { outputLength: 15 }).update('').digest('hex')
  );
  // Check length
  assert.strictEqual(
    crypto.hash('shake128', '', { outputLength: 15 }).length,
    crypto.createHash('shake128', { outputLength: 15 }).update('').digest('hex')
      .length
  );

  assert.strictEqual(
    crypto.hash('shake256', '', { outputLength: 16 }),
    crypto.createHash('shake256', { outputLength: 16 }).update('').digest('hex')
  );
  // Check length
  assert.strictEqual(
    crypto.hash('shake256', '', { outputLength: 16 }).length,
    crypto.createHash('shake256', { outputLength: 16 }).update('').digest('hex')
      .length
  );

  // Large outputLengths.
  assert.strictEqual(
    crypto.hash('shake128', '', { outputLength: 128 }),
    crypto
      .createHash('shake128', { outputLength: 128 }).update('')
      .digest('hex')
  );
  // Check length without encoding
  assert.strictEqual(
    crypto.hash('shake128', '', { outputLength: 128 }).length,
    crypto
      .createHash('shake128', { outputLength: 128 }).update('')
      .digest('hex').length
  );
  assert.strictEqual(
    crypto.hash('shake256', '', { outputLength: 128 }),
    crypto
      .createHash('shake256', { outputLength: 128 }).update('')
      .digest('hex')
  );

  const actual = crypto.hash('shake256', 'The message is shorter than the hash!', { outputLength: 1024 * 1024 });
  const expected = crypto
    .createHash('shake256', {
      outputLength: 1024 * 1024,
    })
    .update('The message is shorter than the hash!')
    .digest('hex');
  assert.strictEqual(actual, expected);

  // Non-XOF hash functions should accept valid outputLength options as well.
  assert.strictEqual(crypto.hash('sha224', '', { outputLength: 28 }),
                     'd14a028c2a3a2bc9476102bb288234c4' +
                     '15a2b01f828ea62ac5b3e42f');

  // Non-XOF hash functions should fail when outputLength isn't their actual outputLength
  assert.throws(() => crypto.hash('sha224', '', { outputLength: 32 }),
                { message: 'Output length 32 is invalid for sha224, which does not support XOF' });
  assert.throws(() => crypto.hash('sha224', '', { outputLength: 0 }),
                { message: 'Output length 0 is invalid for sha224, which does not support XOF' });
}
