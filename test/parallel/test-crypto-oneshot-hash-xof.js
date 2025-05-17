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
    crypto.hash('shake128', '', 'hex'),
    crypto.createHash('shake128').update('').digest('hex')
  );

  assert.strictEqual(
    crypto.hash('shake256', '', 'hex'),
    crypto.createHash('shake256').update('').digest('hex')
  );

  // Short outputLengths.
  assert.strictEqual(crypto.hash('shake128', '', { encoding: 'hex', outputLength: 0 }),
                     crypto.createHash('shake128', { outputLength: 0 })
                     .update('').digest('hex'));

  assert.strictEqual(
    crypto.hash('shake128', '', { outputEncoding: 'hex', outputLength: 5 }),
    crypto.createHash('shake128', { outputLength: 5 }).update('').digest('hex')
  );
  // Check length
  assert.strictEqual(
    crypto.hash('shake128', '', { outputEncoding: 'hex', outputLength: 5 }).length,
    crypto.createHash('shake128', { outputLength: 5 }).update('').digest('hex')
      .length
  );

  assert.strictEqual(
    crypto.hash('shake128', '', { outputEncoding: 'hex', outputLength: 15 }),
    crypto.createHash('shake128', { outputLength: 15 }).update('').digest('hex')
  );
  // Check length
  assert.strictEqual(
    crypto.hash('shake128', '', { outputEncoding: 'hex', outputLength: 15 }).length,
    crypto.createHash('shake128', { outputLength: 15 }).update('').digest('hex')
      .length
  );

  assert.strictEqual(
    crypto.hash('shake256', '', { outputEncoding: 'hex', outputLength: 16 }),
    crypto.createHash('shake256', { outputLength: 16 }).update('').digest('hex')
  );
  // Check length
  assert.strictEqual(
    crypto.hash('shake256', '', { outputEncoding: 'hex', outputLength: 16 }).length,
    crypto.createHash('shake256', { outputLength: 16 }).update('').digest('hex')
      .length
  );

  // Large outputLengths.
  assert.strictEqual(
    crypto.hash('shake128', '', { outputEncoding: 'hex', outputLength: 128 }),
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
  assert.strictEqual(crypto.hash('sha224', '', 'hex', 28),
                     'd14a028c2a3a2bc9476102bb288234c4' +
                     '15a2b01f828ea62ac5b3e42f');
}
