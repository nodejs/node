'use strict';
const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const crypto = require('crypto');

const constants = crypto.constants;

const fixtures = require('../common/fixtures');

// Test certificates
const certPem = fixtures.readKey('rsa_cert.crt');
const keyPem = fixtures.readKey('rsa_private.pem');
const rsaKeySize = 2048;
const rsaPubPem = fixtures.readKey('rsa_public.pem', 'ascii');
const rsaKeyPem = fixtures.readKey('rsa_private.pem', 'ascii');
const rsaKeyPemEncrypted = fixtures.readKey('rsa_private_encrypted.pem',
                                            'ascii');
const dsaPubPem = fixtures.readKey('dsa_public.pem', 'ascii');
const dsaKeyPem = fixtures.readKey('dsa_private.pem', 'ascii');
const dsaKeyPemEncrypted = fixtures.readKey('dsa_private_encrypted.pem',
                                            'ascii');
const rsaPkcs8KeyPem = fixtures.readKey('rsa_private_pkcs8.pem');
const dsaPkcs8KeyPem = fixtures.readKey('dsa_private_pkcs8.pem');

const decryptError = {
  message: 'error:06065064:digital envelope routines:EVP_DecryptFinal_ex:' +
    'bad decrypt',
  code: 'ERR_OSSL_EVP_BAD_DECRYPT',
  reason: 'bad decrypt',
  function: 'EVP_DecryptFinal_ex',
  library: 'digital envelope routines',
};

// Test RSA encryption/decryption
{
  const input = 'I AM THE WALRUS';
  const bufferToEncrypt = Buffer.from(input);
  const bufferPassword = Buffer.from('password');

  let encryptedBuffer = crypto.publicEncrypt(rsaPubPem, bufferToEncrypt);

  let decryptedBuffer = crypto.privateDecrypt(rsaKeyPem, encryptedBuffer);
  assert.strictEqual(decryptedBuffer.toString(), input);

  decryptedBuffer = crypto.privateDecrypt(rsaPkcs8KeyPem, encryptedBuffer);
  assert.strictEqual(decryptedBuffer.toString(), input);

  let decryptedBufferWithPassword = crypto.privateDecrypt({
    key: rsaKeyPemEncrypted,
    passphrase: 'password'
  }, encryptedBuffer);
  assert.strictEqual(decryptedBufferWithPassword.toString(), input);

  encryptedBuffer = crypto.publicEncrypt({
    key: rsaKeyPemEncrypted,
    passphrase: 'password'
  }, bufferToEncrypt);

  decryptedBufferWithPassword = crypto.privateDecrypt({
    key: rsaKeyPemEncrypted,
    passphrase: 'password'
  }, encryptedBuffer);
  assert.strictEqual(decryptedBufferWithPassword.toString(), input);

  encryptedBuffer = crypto.privateEncrypt({
    key: rsaKeyPemEncrypted,
    passphrase: bufferPassword
  }, bufferToEncrypt);

  decryptedBufferWithPassword = crypto.publicDecrypt({
    key: rsaKeyPemEncrypted,
    passphrase: bufferPassword
  }, encryptedBuffer);
  assert.strictEqual(decryptedBufferWithPassword.toString(), input);

  // Now with explicit RSA_PKCS1_PADDING.
  encryptedBuffer = crypto.privateEncrypt({
    padding: crypto.constants.RSA_PKCS1_PADDING,
    key: rsaKeyPemEncrypted,
    passphrase: bufferPassword
  }, bufferToEncrypt);

  decryptedBufferWithPassword = crypto.publicDecrypt({
    padding: crypto.constants.RSA_PKCS1_PADDING,
    key: rsaKeyPemEncrypted,
    passphrase: bufferPassword
  }, encryptedBuffer);
  assert.strictEqual(decryptedBufferWithPassword.toString(), input);

  // Omitting padding should be okay because RSA_PKCS1_PADDING is the default.
  decryptedBufferWithPassword = crypto.publicDecrypt({
    key: rsaKeyPemEncrypted,
    passphrase: bufferPassword
  }, encryptedBuffer);
  assert.strictEqual(decryptedBufferWithPassword.toString(), input);

  // Now with RSA_NO_PADDING. Plaintext needs to match key size.
  const plaintext = 'x'.repeat(rsaKeySize / 8);
  encryptedBuffer = crypto.privateEncrypt({
    padding: crypto.constants.RSA_NO_PADDING,
    key: rsaKeyPemEncrypted,
    passphrase: bufferPassword
  }, Buffer.from(plaintext));

  decryptedBufferWithPassword = crypto.publicDecrypt({
    padding: crypto.constants.RSA_NO_PADDING,
    key: rsaKeyPemEncrypted,
    passphrase: bufferPassword
  }, encryptedBuffer);
  assert.strictEqual(decryptedBufferWithPassword.toString(), plaintext);

  encryptedBuffer = crypto.publicEncrypt(certPem, bufferToEncrypt);

  decryptedBuffer = crypto.privateDecrypt(keyPem, encryptedBuffer);
  assert.strictEqual(decryptedBuffer.toString(), input);

  encryptedBuffer = crypto.publicEncrypt(keyPem, bufferToEncrypt);

  decryptedBuffer = crypto.privateDecrypt(keyPem, encryptedBuffer);
  assert.strictEqual(decryptedBuffer.toString(), input);

  encryptedBuffer = crypto.privateEncrypt(keyPem, bufferToEncrypt);

  decryptedBuffer = crypto.publicDecrypt(keyPem, encryptedBuffer);
  assert.strictEqual(decryptedBuffer.toString(), input);

  assert.throws(() => {
    crypto.privateDecrypt({
      key: rsaKeyPemEncrypted,
      passphrase: 'wrong'
    }, bufferToEncrypt);
  }, decryptError);

  assert.throws(() => {
    crypto.publicEncrypt({
      key: rsaKeyPemEncrypted,
      passphrase: 'wrong'
    }, encryptedBuffer);
  }, decryptError);

  encryptedBuffer = crypto.privateEncrypt({
    key: rsaKeyPemEncrypted,
    passphrase: Buffer.from('password')
  }, bufferToEncrypt);

  assert.throws(() => {
    crypto.publicDecrypt({
      key: rsaKeyPemEncrypted,
      passphrase: Buffer.from('wrong')
    }, encryptedBuffer);
  }, decryptError);
}

function test_rsa(padding, encryptOaepHash, decryptOaepHash) {
  const size = (padding === 'RSA_NO_PADDING') ? rsaKeySize / 8 : 32;
  const input = Buffer.allocUnsafe(size);
  for (let i = 0; i < input.length; i++)
    input[i] = (i * 7 + 11) & 0xff;
  const bufferToEncrypt = Buffer.from(input);

  padding = constants[padding];

  const encryptedBuffer = crypto.publicEncrypt({
    key: rsaPubPem,
    padding: padding,
    oaepHash: encryptOaepHash
  }, bufferToEncrypt);

  let decryptedBuffer = crypto.privateDecrypt({
    key: rsaKeyPem,
    padding: padding,
    oaepHash: decryptOaepHash
  }, encryptedBuffer);
  assert.deepStrictEqual(decryptedBuffer, input);

  decryptedBuffer = crypto.privateDecrypt({
    key: rsaPkcs8KeyPem,
    padding: padding,
    oaepHash: decryptOaepHash
  }, encryptedBuffer);
  assert.deepStrictEqual(decryptedBuffer, input);
}

test_rsa('RSA_NO_PADDING');
test_rsa('RSA_PKCS1_PADDING');
test_rsa('RSA_PKCS1_OAEP_PADDING');

// Test OAEP with different hash functions.
test_rsa('RSA_PKCS1_OAEP_PADDING', undefined, 'sha1');
test_rsa('RSA_PKCS1_OAEP_PADDING', 'sha1', undefined);
test_rsa('RSA_PKCS1_OAEP_PADDING', 'sha256', 'sha256');
test_rsa('RSA_PKCS1_OAEP_PADDING', 'sha512', 'sha512');
common.expectsError(() => {
  test_rsa('RSA_PKCS1_OAEP_PADDING', 'sha256', 'sha512');
}, {
  code: 'ERR_OSSL_RSA_OAEP_DECODING_ERROR'
});

// The following RSA-OAEP test cases were created using the WebCrypto API to
// ensure compatibility when using non-SHA1 hash functions.
{
  function testDecrypt(oaepHash, ciphertext) {
    const decrypted = crypto.privateDecrypt({
      key: rsaPkcs8KeyPem,
      oaepHash
    }, Buffer.from(ciphertext, 'hex'));

    assert.strictEqual(decrypted.toString('utf8'), 'Hello Node.js');
  }

  testDecrypt(undefined, '16ece59cf985a8cf1a3434e4b9707c922c20638fdf9abf7e5dc' +
                         '7943f4136899348c54116d15b2c17563b9c7143f9d5b85b4561' +
                         '5ad0598ea6d21c900f3957b65400612306a9bebae441f005646' +
                         'f7a7c97129a103ab54e777168ef966514adb17786b968ea0ff4' +
                         '30a524904c4a11c683764b7c8dbb60df0952768381cdba4d665' +
                         'e5006034393a10d56d33e75b2714db824a18da46441ef7f94a3' +
                         '4a7058c0bbad0394083a038558bcc6dd370f8e518e1bd8d73b2' +
                         '96fc51d77da44799e4ee774926ded7910e8768f92db76f63107' +
                         '338d33354b735d3ad094240dbd7ffdfda27ef0255306dcf4a64' +
                         '62849492abd1a97fdd37743ff87c4d2ec89866c5cdbb696bd2b' +
                         '30');
  testDecrypt('sha1', '16ece59cf985a8cf1a3434e4b9707c922c20638fdf9abf7e5dc794' +
                      '3f4136899348c54116d15b2c17563b9c7143f9d5b85b45615ad059' +
                      '8ea6d21c900f3957b65400612306a9bebae441f005646f7a7c9712' +
                      '9a103ab54e777168ef966514adb17786b968ea0ff430a524904c4a' +
                      '11c683764b7c8dbb60df0952768381cdba4d665e5006034393a10d' +
                      '56d33e75b2714db824a18da46441ef7f94a34a7058c0bbad039408' +
                      '3a038558bcc6dd370f8e518e1bd8d73b296fc51d77da44799e4ee7' +
                      '74926ded7910e8768f92db76f63107338d33354b735d3ad094240d' +
                      'bd7ffdfda27ef0255306dcf4a6462849492abd1a97fdd37743ff87' +
                      'c4d2ec89866c5cdbb696bd2b30');
  testDecrypt('sha256', '16ccf09afe5eb0130182b9fc1ca4af61a38e772047cac42146bf' +
                        'a0fa5879aa9639203e4d01442d212ff95bddfbe4661222215a2e' +
                        '91908c37ab926edea7cfc53f83357bc27f86af0f5f2818ae141f' +
                        '4e9e934d4e66189aff30f062c9c3f6eb9bc495a59082cb978f99' +
                        'b56ce5fa530a8469e46129258e5c42897cb194b6805e936e5cbb' +
                        'eaa535bad6b1d3cdfc92119b7dd325a2e6d2979e316bdacc9f80' +
                        'e29c7bbdf6846d738e380deadcb48df8c1e8aabf7a9dd2f8c71d' +
                        '6681dbec7dcadc01887c51288674268796bc77fdf8f1c94c9ca5' +
                        '0b1cc7cddbaf4e56cb151d23e2c699d2844c0104ee2e7e9dcdb9' +
                        '07cfab43339120a40c59ca54f32b8d21b48a29656c77');
  testDecrypt('sha512', '831b72e8dd91841729ecbddf2647d6f19dc0094734f8803d8c65' +
                        '1b5655a12ae6156b74d9b594bcc0eacd002728380b94f46e8657' +
                        'f130f354e03b6e7815ee257eda78dba296d67d24410c31c48e58' +
                        '75cc79e4bde594b412be5f357f57a7ac1f1d18b718e408df162d' +
                        '1795508e6a0616192b647ad942ea068a44fb2b323d35a3a61b92' +
                        '6feb105d6c0b2a8fc8050222d1cf4a9e44da1f95bbc677fd6437' +
                        '49c6c89ac551d072f04cd9320c97a8d94755c8a804954c082bed' +
                        '7fa59199a00aca154c14a7b584b63c538daf9b9c7c90abfca193' +
                        '87d2131f9d9b9ecfc8672249c33144d1be3bfc41558a13f99466' +
                        '3661a3af24fd0a97619d508db36f5fc131af86fc68cf');
}

// Test invalid oaepHash options.
for (const fn of [crypto.publicEncrypt, crypto.privateDecrypt]) {
  assert.throws(() => {
    fn({
      key: rsaPubPem,
      oaepHash: 'Hello world'
    }, Buffer.alloc(10));
  }, {
    code: 'ERR_OSSL_EVP_INVALID_DIGEST'
  });

  for (const oaepHash of [0, false, null, Symbol(), () => {}]) {
    common.expectsError(() => {
      fn({
        key: rsaPubPem,
        oaepHash
      }, Buffer.alloc(10));
    }, {
      code: 'ERR_INVALID_ARG_TYPE'
    });
  }
}

// Test RSA key signing/verification
let rsaSign = crypto.createSign('SHA1');
let rsaVerify = crypto.createVerify('SHA1');
assert.ok(rsaSign);
assert.ok(rsaVerify);

const expectedSignature = fixtures.readKey(
  'rsa_public_sha1_signature_signedby_rsa_private_pkcs8.sha1',
  'hex'
);

rsaSign.update(rsaPubPem);
let rsaSignature = rsaSign.sign(rsaKeyPem, 'hex');
assert.strictEqual(rsaSignature, expectedSignature);

rsaVerify.update(rsaPubPem);
assert.strictEqual(rsaVerify.verify(rsaPubPem, rsaSignature, 'hex'), true);

// Test RSA PKCS#8 key signing/verification
rsaSign = crypto.createSign('SHA1');
rsaSign.update(rsaPubPem);
rsaSignature = rsaSign.sign(rsaPkcs8KeyPem, 'hex');
assert.strictEqual(rsaSignature, expectedSignature);

rsaVerify = crypto.createVerify('SHA1');
rsaVerify.update(rsaPubPem);
assert.strictEqual(rsaVerify.verify(rsaPubPem, rsaSignature, 'hex'), true);

// Test RSA key signing/verification with encrypted key
rsaSign = crypto.createSign('SHA1');
rsaSign.update(rsaPubPem);
const signOptions = { key: rsaKeyPemEncrypted, passphrase: 'password' };
rsaSignature = rsaSign.sign(signOptions, 'hex');
assert.strictEqual(rsaSignature, expectedSignature);

rsaVerify = crypto.createVerify('SHA1');
rsaVerify.update(rsaPubPem);
assert.strictEqual(rsaVerify.verify(rsaPubPem, rsaSignature, 'hex'), true);

rsaSign = crypto.createSign('SHA1');
rsaSign.update(rsaPubPem);
assert.throws(() => {
  const signOptions = { key: rsaKeyPemEncrypted, passphrase: 'wrong' };
  rsaSign.sign(signOptions, 'hex');
}, decryptError);

//
// Test RSA signing and verification
//
{
  const privateKey = fixtures.readKey('rsa_private_b.pem');
  const publicKey = fixtures.readKey('rsa_public_b.pem');

  const input = 'I AM THE WALRUS';

  const signature = fixtures.readKey(
    'I_AM_THE_WALRUS_sha256_signature_signedby_rsa_private_b.sha256',
    'hex'
  );

  const sign = crypto.createSign('SHA256');
  sign.update(input);

  const output = sign.sign(privateKey, 'hex');
  assert.strictEqual(output, signature);

  const verify = crypto.createVerify('SHA256');
  verify.update(input);

  assert.strictEqual(verify.verify(publicKey, signature, 'hex'), true);

  // Test the legacy signature algorithm name.
  const sign2 = crypto.createSign('RSA-SHA256');
  sign2.update(input);

  const output2 = sign2.sign(privateKey, 'hex');
  assert.strictEqual(output2, signature);

  const verify2 = crypto.createVerify('SHA256');
  verify2.update(input);

  assert.strictEqual(verify2.verify(publicKey, signature, 'hex'), true);
}


//
// Test DSA signing and verification
//
{
  const input = 'I AM THE WALRUS';

  // DSA signatures vary across runs so there is no static string to verify
  // against.
  const sign = crypto.createSign('SHA1');
  sign.update(input);
  const signature = sign.sign(dsaKeyPem, 'hex');

  const verify = crypto.createVerify('SHA1');
  verify.update(input);

  assert.strictEqual(verify.verify(dsaPubPem, signature, 'hex'), true);

  // Test the legacy 'DSS1' name.
  const sign2 = crypto.createSign('DSS1');
  sign2.update(input);
  const signature2 = sign2.sign(dsaKeyPem, 'hex');

  const verify2 = crypto.createVerify('DSS1');
  verify2.update(input);

  assert.strictEqual(verify2.verify(dsaPubPem, signature2, 'hex'), true);
}


//
// Test DSA signing and verification with PKCS#8 private key
//
{
  const input = 'I AM THE WALRUS';

  // DSA signatures vary across runs so there is no static string to verify
  // against.
  const sign = crypto.createSign('SHA1');
  sign.update(input);
  const signature = sign.sign(dsaPkcs8KeyPem, 'hex');

  const verify = crypto.createVerify('SHA1');
  verify.update(input);

  assert.strictEqual(verify.verify(dsaPubPem, signature, 'hex'), true);
}


//
// Test DSA signing and verification with encrypted key
//
const input = 'I AM THE WALRUS';

{
  const sign = crypto.createSign('SHA1');
  sign.update(input);
  assert.throws(() => {
    sign.sign({ key: dsaKeyPemEncrypted, passphrase: 'wrong' }, 'hex');
  }, decryptError);
}

{
  // DSA signatures vary across runs so there is no static string to verify
  // against.
  const sign = crypto.createSign('SHA1');
  sign.update(input);
  const signOptions = { key: dsaKeyPemEncrypted, passphrase: 'password' };
  const signature = sign.sign(signOptions, 'hex');

  const verify = crypto.createVerify('SHA1');
  verify.update(input);

  assert.strictEqual(verify.verify(dsaPubPem, signature, 'hex'), true);
}
