'use strict';
const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const crypto = require('crypto');
const { hasOpenSSL, hasFIPS } = require('../common/crypto');
const isFipsEnabled = crypto.getFips() === 1;
const fips3 = hasFIPS(3);

function testCipher1(key, iv) {
  // Test encryption and decryption with explicit key and iv
  const plaintext =
          '32|RmVZZkFUVmpRRkp0TmJaUm56ZU9qcnJkaXNNWVNpTTU*|iXmckfRWZBGWWELw' +
          'eCBsThSsfUHLeRe0KCsK8ooHgxie0zOINpXxfZi/oNG7uq9JWFVCk70gfzQH8ZUJ' +
          'jAfaFg**';
  const cipher = crypto.createCipheriv('des-ede3-cbc', key, iv);
  let ciph = cipher.update(plaintext, 'utf8', 'hex');
  ciph += cipher.final('hex');

  const decipher = crypto.createDecipheriv('des-ede3-cbc', key, iv);
  let txt = decipher.update(ciph, 'hex', 'utf8');
  txt += decipher.final('utf8');

  assert.strictEqual(txt, plaintext,
                     `encryption/decryption with key ${key} and iv ${iv}`);

  // Streaming cipher interface
  // NB: In real life, it's not guaranteed that you can get all of it
  // in a single read() like this.  But in this case, we know it's
  // quite small, so there's no harm.
  const cStream = crypto.createCipheriv('des-ede3-cbc', key, iv);
  cStream.end(plaintext);
  ciph = cStream.read(cStream.readableLength);

  const dStream = crypto.createDecipheriv('des-ede3-cbc', key, iv);
  dStream.end(ciph);
  txt = dStream.read(dStream.readableLength).toString('utf8');

  assert.strictEqual(txt, plaintext,
                     `streaming cipher with key ${key} and iv ${iv}`);
}


function testCipher2(key, iv) {
  // Test encryption and decryption with explicit key and iv
  const plaintext =
          '32|RmVZZkFUVmpRRkp0TmJaUm56ZU9qcnJkaXNNWVNpTTU*|iXmckfRWZBGWWELw' +
          'eCBsThSsfUHLeRe0KCsK8ooHgxie0zOINpXxfZi/oNG7uq9JWFVCk70gfzQH8ZUJ' +
          'jAfaFg**';
  const cipher = crypto.createCipheriv('des-ede3-cbc', key, iv);
  let ciph = cipher.update(plaintext, 'utf8', 'buffer');
  ciph = Buffer.concat([ciph, cipher.final('buffer')]);

  const decipher = crypto.createDecipheriv('des-ede3-cbc', key, iv);
  let txt = decipher.update(ciph, 'buffer', 'utf8');
  txt += decipher.final('utf8');

  assert.strictEqual(txt, plaintext,
                     `encryption/decryption with key ${key} and iv ${iv}`);
}


function testCipher3(key, iv) {
  if (!crypto.getCiphers().includes('id-aes128-wrap')) {
    common.printSkipMessage(`unsupported id-aes128-wrap test`);
    return;
  }
  // Test encryption and decryption with explicit key and iv.
  // AES Key Wrap test vector comes from RFC3394
  const plaintext = Buffer.from('00112233445566778899AABBCCDDEEFF', 'hex');

  const cipher = crypto.createCipheriv('id-aes128-wrap', key, iv);
  let ciph = cipher.update(plaintext, 'utf8', 'buffer');
  ciph = Buffer.concat([ciph, cipher.final('buffer')]);
  const ciph2 = Buffer.from('1FA68B0A8112B447AEF34BD8FB5A7B829D3E862371D2CFE5',
                            'hex');
  assert(ciph.equals(ciph2));
  const decipher = crypto.createDecipheriv('id-aes128-wrap', key, iv);
  let deciph = decipher.update(ciph, 'buffer');
  deciph = Buffer.concat([deciph, decipher.final()]);

  assert(deciph.equals(plaintext),
         `encryption/decryption with key ${key} and iv ${iv}`);
}

function testSm4Xts() {
  if (!crypto.getCiphers().includes('sm4-xts')) {
    common.printSkipMessage('unsupported sm4-xts test');
    return;
  }

  // GB/T 17964-2021
  const key = Buffer.from(
    '2B7E151628AED2A6ABF7158809CF4F3C' +
    '000102030405060708090A0B0C0D0E0F', 'hex');
  const iv = Buffer.from('F0F1F2F3F4F5F6F7F8F9FAFBFCFDFEFF', 'hex');
  const plaintext = Buffer.from(
    '6BC1BEE22E409F96E93D7E117393172A' +
    'AE2D8A571E03AC9C9EB76FAC45AF8E51' +
    '30C81C46A35CE411E5FBC1191A0A52EF' +
    'F69F2445DF4F9B17', 'hex');
  const expected = Buffer.from(
    'E9538251C71D7B80BBE4483FEF497BD1' +
    '2C5C581BD6242FC51E08964FB4F60FDB' +
    '0BA42F63499279213D318D2C11F6886E' +
    '903BE7F93A1B3479', 'hex');

  const cipher = crypto.createCipheriv('sm4-xts', key, iv);
  const ciphertext = Buffer.concat([
    cipher.update(plaintext),
    cipher.final(),
  ]);
  assert.deepStrictEqual(ciphertext, expected);

  const decipher = crypto.createDecipheriv('sm4-xts', key, iv);
  const decrypted = Buffer.concat([
    decipher.update(ciphertext),
    decipher.final(),
  ]);
  assert.deepStrictEqual(decrypted, plaintext);
}

{
  const Cipheriv = crypto.Cipheriv;
  const algorithm = fips3 ? 'aes-128-cbc' : 'des-ede3-cbc';
  const key = fips3 ?
    '1234567890123456' : '123456789012345678901234';
  const iv = fips3 ? '1234567890123456' : '12345678';

  const instance = Cipheriv(algorithm, key, iv);
  assert(instance instanceof Cipheriv, 'Cipheriv is expected to return a new ' +
                                       'instance when called without `new`');

  assert.throws(
    () => crypto.createCipheriv(null),
    {
      code: 'ERR_INVALID_ARG_TYPE',
      name: 'TypeError',
      message: 'The "cipher" argument must be of type string. ' +
               'Received null'
    });

  assert.throws(
    () => crypto.createCipheriv('des-ede3-cbc', null),
    {
      code: 'ERR_INVALID_ARG_TYPE',
      name: 'TypeError',
    });

  assert.throws(
    () => crypto.createCipheriv('des-ede3-cbc', key, 10),
    {
      code: 'ERR_INVALID_ARG_TYPE',
      name: 'TypeError',
    });
}

{
  const Decipheriv = crypto.Decipheriv;
  const algorithm = fips3 ? 'aes-128-cbc' : 'des-ede3-cbc';
  const key = fips3 ?
    '1234567890123456' : '123456789012345678901234';
  const iv = fips3 ? '1234567890123456' : '12345678';

  const instance = Decipheriv(algorithm, key, iv);
  assert(instance instanceof Decipheriv, 'Decipheriv expected to return a new' +
                                         ' instance when called without `new`');

  assert.throws(
    () => crypto.createDecipheriv(null),
    {
      code: 'ERR_INVALID_ARG_TYPE',
      name: 'TypeError',
      message: 'The "cipher" argument must be of type string. ' +
               'Received null'
    });

  assert.throws(
    () => crypto.createDecipheriv('des-ede3-cbc', null),
    {
      code: 'ERR_INVALID_ARG_TYPE',
      name: 'TypeError',
    });

  assert.throws(
    () => crypto.createDecipheriv('des-ede3-cbc', key, 10),
    {
      code: 'ERR_INVALID_ARG_TYPE',
      name: 'TypeError',
    });
}

testCipher1('0123456789abcd0123456789', '12345678');
testCipher1('0123456789abcd0123456789', Buffer.from('12345678'));
testCipher1(Buffer.from('0123456789abcd0123456789'), '12345678');
testCipher1(
  Buffer.from('0123456789abcd0123456789'), Buffer.from('12345678'));
testCipher2(
  Buffer.from('0123456789abcd0123456789'), Buffer.from('12345678'));

if (!isFipsEnabled) {
  testCipher3(Buffer.from('000102030405060708090A0B0C0D0E0F', 'hex'),
              Buffer.from('A6A6A6A6A6A6A6A6', 'hex'));
}
testSm4Xts();

// Zero-sized IV or null should be accepted in ECB mode.
crypto.createCipheriv('aes-128-ecb', Buffer.alloc(16), Buffer.alloc(0));
crypto.createCipheriv('aes-128-ecb', Buffer.alloc(16), null);

const errMessage = /Invalid initialization vector/;

// But non-empty IVs should be rejected.
for (let n = 1; n < 256; n += 1) {
  assert.throws(
    () => crypto.createCipheriv('aes-128-ecb', Buffer.alloc(16),
                                Buffer.alloc(n)),
    errMessage);
}

// And so should undefined be (regardless of mode).
assert.throws(
  () => crypto.createCipheriv('aes-128-ecb', Buffer.alloc(16)),
  { code: 'ERR_INVALID_ARG_TYPE' });
assert.throws(
  () => crypto.createCipheriv('aes-128-ecb', Buffer.alloc(16), undefined),
  { code: 'ERR_INVALID_ARG_TYPE' });

// Correctly sized IV should be accepted in CBC mode.
crypto.createCipheriv('aes-128-cbc', Buffer.alloc(16), Buffer.alloc(16));

// But all other IV lengths should be rejected.
for (let n = 0; n < 256; n += 1) {
  if (n === 16) continue;
  assert.throws(
    () => crypto.createCipheriv('aes-128-cbc', Buffer.alloc(16),
                                Buffer.alloc(n)),
    errMessage);
}

// And so should null be.
assert.throws(() => {
  crypto.createCipheriv('aes-128-cbc', Buffer.alloc(16), null);
}, /Invalid initialization vector/);

// Zero-sized IV should be rejected in GCM mode.
assert.throws(
  () => crypto.createCipheriv('aes-128-gcm', Buffer.alloc(16),
                              Buffer.alloc(0)),
  errMessage);

// But all other IV lengths should be accepted.
const minIvLength = hasOpenSSL(3) ? 8 : 1;
const maxIvLength = hasOpenSSL(3) ? 64 : 256;
for (let n = minIvLength; n < maxIvLength; n += 1) {
  if (isFipsEnabled && n < 12) continue;
  crypto.createCipheriv('aes-128-gcm', Buffer.alloc(16), Buffer.alloc(n));
}

{
  // Passing an invalid cipher name should throw.
  assert.throws(
    () => crypto.createCipheriv('aes-127', Buffer.alloc(16), null),
    {
      name: 'Error',
      code: 'ERR_CRYPTO_UNKNOWN_CIPHER',
      message: 'Unknown cipher'
    });

  // Passing a key with an invalid length should throw.
  assert.throws(
    () => crypto.createCipheriv('aes-128-ecb', Buffer.alloc(17), null),
    /Invalid key length/);
}

{
  // https://github.com/nodejs/node/issues/45757
  // eslint-disable-next-line no-restricted-syntax
  assert.throws(() =>
    crypto.createCipheriv('aes-128-gcm', Buffer.alloc(16), Buffer.alloc(12))
    .update(Buffer.allocUnsafeSlow(2 ** 31 - 1)));
}
