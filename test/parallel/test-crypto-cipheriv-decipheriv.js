'use strict';
const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const crypto = require('crypto');

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

  // streaming cipher interface
  // NB: In real life, it's not guaranteed that you can get all of it
  // in a single read() like this.  But in this case, we know it's
  // quite small, so there's no harm.
  const cStream = crypto.createCipheriv('des-ede3-cbc', key, iv);
  cStream.end(plaintext);
  ciph = cStream.read();

  const dStream = crypto.createDecipheriv('des-ede3-cbc', key, iv);
  dStream.end(ciph);
  txt = dStream.read().toString('utf8');

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

{
  const Cipheriv = crypto.Cipheriv;
  const key = '123456789012345678901234';
  const iv = '12345678';

  const instance = Cipheriv('des-ede3-cbc', key, iv);
  assert(instance instanceof Cipheriv, 'Cipheriv is expected to return a new ' +
                                       'instance when called without `new`');

  common.expectsError(
    () => crypto.createCipheriv(null),
    {
      code: 'ERR_INVALID_ARG_TYPE',
      type: TypeError,
      message: 'The "cipher" argument must be of type string'
    });

  common.expectsError(
    () => crypto.createCipheriv('des-ede3-cbc', null),
    {
      code: 'ERR_INVALID_ARG_TYPE',
      type: TypeError,
      message: 'The "key" argument must be one of type string, Buffer, ' +
               'TypedArray, or DataView'
    });

  common.expectsError(
    () => crypto.createCipheriv('des-ede3-cbc', key, null),
    {
      code: 'ERR_INVALID_ARG_TYPE',
      type: TypeError,
      message: 'The "iv" argument must be one of type string, Buffer, ' +
               'TypedArray, or DataView'
    });
}

{
  const Decipheriv = crypto.Decipheriv;
  const key = '123456789012345678901234';
  const iv = '12345678';

  const instance = Decipheriv('des-ede3-cbc', key, iv);
  assert(instance instanceof Decipheriv, 'Decipheriv expected to return a new' +
                                         ' instance when called without `new`');

  common.expectsError(
    () => crypto.createDecipheriv(null),
    {
      code: 'ERR_INVALID_ARG_TYPE',
      type: TypeError,
      message: 'The "cipher" argument must be of type string'
    });

  common.expectsError(
    () => crypto.createDecipheriv('des-ede3-cbc', null),
    {
      code: 'ERR_INVALID_ARG_TYPE',
      type: TypeError,
      message: 'The "key" argument must be one of type string, Buffer, ' +
               'TypedArray, or DataView'
    });

  common.expectsError(
    () => crypto.createDecipheriv('des-ede3-cbc', key, null),
    {
      code: 'ERR_INVALID_ARG_TYPE',
      type: TypeError,
      message: 'The "iv" argument must be one of type string, Buffer, ' +
               'TypedArray, or DataView'
    });
}

testCipher1('0123456789abcd0123456789', '12345678');
testCipher1('0123456789abcd0123456789', Buffer.from('12345678'));
testCipher1(Buffer.from('0123456789abcd0123456789'), '12345678');
testCipher1(Buffer.from('0123456789abcd0123456789'), Buffer.from('12345678'));
testCipher2(Buffer.from('0123456789abcd0123456789'), Buffer.from('12345678'));

if (!common.hasFipsCrypto) {
  testCipher3(Buffer.from('000102030405060708090A0B0C0D0E0F', 'hex'),
              Buffer.from('A6A6A6A6A6A6A6A6', 'hex'));
}

// Zero-sized IV should be accepted in ECB mode.
crypto.createCipheriv('aes-128-ecb', Buffer.alloc(16), Buffer.alloc(0));

const errMessage = /Invalid IV length/;

// But non-empty IVs should be rejected.
for (let n = 1; n < 256; n += 1) {
  assert.throws(
    () => crypto.createCipheriv('aes-128-ecb', Buffer.alloc(16),
                                Buffer.alloc(n)),
    errMessage);
}

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

// Zero-sized IV should be rejected in GCM mode.
assert.throws(
  () => crypto.createCipheriv('aes-128-gcm', Buffer.alloc(16),
                              Buffer.alloc(0)),
  errMessage);

// But all other IV lengths should be accepted.
for (let n = 1; n < 256; n += 1) {
  if (common.hasFipsCrypto && n < 12) continue;
  crypto.createCipheriv('aes-128-gcm', Buffer.alloc(16), Buffer.alloc(n));
}
