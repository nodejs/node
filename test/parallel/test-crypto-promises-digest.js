'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');

const {
  digest,
} = require('crypto/promises');

async function testDigest() {
  // Test cryptoPromises.digest

  // Test XOF hash functions and the outputLength option.
  {
    // Default outputLengths.
    assert.deepStrictEqual(
      await digest('shake128', Buffer.alloc(0)),
      Buffer.from('7f9c2ba4e88f827d616045507605853e', 'hex'));
    assert.deepStrictEqual(
      await digest('shake256', Buffer.alloc(0)),
      Buffer.from('46b9dd2b0ba88d13233b3feb743eeb24' +
                  '3fcd52ea62b81b82b50c27646ed5762f', 'hex'));

    // Short outputLengths.
    assert.deepStrictEqual(
      await digest('shake128', Buffer.alloc(0), { outputLength: 0 }),
      Buffer.alloc(0));
    assert.deepStrictEqual(
      await digest('shake128', Buffer.alloc(0), { outputLength: 5 }),
      Buffer.from('7f9c2ba4e8', 'hex'));
    assert.deepStrictEqual(
      await digest('shake128', Buffer.alloc(0), { outputLength: 15 }),
      Buffer.from('7f9c2ba4e88f827d61604550760585', 'hex'));
    assert.deepStrictEqual(
      await digest('shake256', Buffer.alloc(0), { outputLength: 16 }),
      Buffer.from('46b9dd2b0ba88d13233b3feb743eeb24', 'hex'));

    // Large outputLengths.
    assert.deepStrictEqual(
      await digest('shake128', Buffer.alloc(0), { outputLength: 128 }),
      Buffer.from('7f9c2ba4e88f827d616045507605853e' +
      'd73b8093f6efbc88eb1a6eacfa66ef26' +
      '3cb1eea988004b93103cfb0aeefd2a68' +
      '6e01fa4a58e8a3639ca8a1e3f9ae57e2' +
      '35b8cc873c23dc62b8d260169afa2f75' +
      'ab916a58d974918835d25e6a435085b2' +
      'badfd6dfaac359a5efbb7bcc4b59d538' +
      'df9a04302e10c8bc1cbf1a0b3a5120ea', 'hex'));

    const superLongHash = await digest(
      'shake256',
      Buffer.from('The message is shorter than the hash!'),
      { outputLength: 1024 * 1024 });
    assert.strictEqual(superLongHash.length, 1024 * 1024);

    const end = Buffer.from('193414035ddba77bf7bba97981e656ec', 'hex');
    assert.strictEqual(
      superLongHash.indexOf(end),
      superLongHash.length - end.length);
    assert.strictEqual(
      superLongHash.indexOf(
        Buffer.from('a2a28dbc49cfd6e5d6ceea3d03e77748', 'hex')
      ),
      0);

    // Non-XOF hash functions should accept valid outputLength options as well.
    assert.deepStrictEqual(
      await digest('sha224', Buffer.alloc(0), { outputLength: 28 }),
      Buffer.from('d14a028c2a3a2bc9476102bb288234c4' +
      '15a2b01f828ea62ac5b3e42f', 'hex'));

    // Passing invalid sizes should throw during creation.
    // TODO(@jasnell): rejects ERR_CRYPTO_INVALID_DIGEST instead of
    // ERR_OSSL_EVP_NOT_XOF_OR_INVALID_LENGTH
    //
    // await assert.rejects(
    //   () => digest('sha256', Buffer.alloc(0), { outputLength: 28 }),
    //   { code: 'ERR_OSSL_EVP_NOT_XOF_OR_INVALID_LENGTH' });

    for (const outputLength of [null, {}, 'foo', false]) {
      await assert.rejects(
        () => digest('sha256', Buffer.alloc(0), { outputLength }),
        { code: 'ERR_INVALID_ARG_TYPE' });
    }

    for (const outputLength of [-1, .5, Infinity, 2 ** 90]) {
      await assert.rejects(
        () => digest('sha256', Buffer.alloc(0), { outputLength }),
        { code: 'ERR_OUT_OF_RANGE' });
    }
  }
}

testDigest().then(common.mustCall());
