'use strict';
const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const crypto = require('crypto');

const constants = crypto.constants;

const fixtures = require('../common/fixtures');

// Test certificates
const certPem = fixtures.readSync('test_cert.pem', 'ascii');
const keyPem = fixtures.readSync('test_key.pem', 'ascii');
const rsaPubPem = fixtures.readSync('test_rsa_pubkey.pem', 'ascii');
const rsaKeyPem = fixtures.readSync('test_rsa_privkey.pem', 'ascii');
const rsaKeyPemEncrypted = fixtures.readSync('test_rsa_privkey_encrypted.pem',
                                             'ascii');
const dsaPubPem = fixtures.readSync('test_dsa_pubkey.pem', 'ascii');
const dsaKeyPem = fixtures.readSync('test_dsa_privkey.pem', 'ascii');
const dsaKeyPemEncrypted = fixtures.readSync('test_dsa_privkey_encrypted.pem',
                                             'ascii');

const decryptError =
  /^Error: error:06065064:digital envelope routines:EVP_DecryptFinal_ex:bad decrypt$/;

// Test RSA encryption/decryption
{
  const input = 'I AM THE WALRUS';
  const bufferToEncrypt = Buffer.from(input);

  let encryptedBuffer = crypto.publicEncrypt(rsaPubPem, bufferToEncrypt);

  let decryptedBuffer = crypto.privateDecrypt(rsaKeyPem, encryptedBuffer);
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
    passphrase: Buffer.from('password')
  }, bufferToEncrypt);

  decryptedBufferWithPassword = crypto.publicDecrypt({
    key: rsaKeyPemEncrypted,
    passphrase: Buffer.from('password')
  }, encryptedBuffer);
  assert.strictEqual(decryptedBufferWithPassword.toString(), input);

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

function test_rsa(padding) {
  const size = (padding === 'RSA_NO_PADDING') ? 1024 / 8 : 32;
  const input = Buffer.allocUnsafe(size);
  for (let i = 0; i < input.length; i++)
    input[i] = (i * 7 + 11) & 0xff;
  const bufferToEncrypt = Buffer.from(input);

  padding = constants[padding];

  const encryptedBuffer = crypto.publicEncrypt({
    key: rsaPubPem,
    padding: padding
  }, bufferToEncrypt);

  const decryptedBuffer = crypto.privateDecrypt({
    key: rsaKeyPem,
    padding: padding
  }, encryptedBuffer);
  assert.deepStrictEqual(decryptedBuffer, input);
}

test_rsa('RSA_NO_PADDING');
test_rsa('RSA_PKCS1_PADDING');
test_rsa('RSA_PKCS1_OAEP_PADDING');

// Test RSA key signing/verification
let rsaSign = crypto.createSign('SHA1');
let rsaVerify = crypto.createVerify('SHA1');
assert.ok(rsaSign);
assert.ok(rsaVerify);

const expectedSignature =
      '5c50e3145c4e2497aadb0eabc83b342d0b0021ece0d4c4a064b7c' +
      '8f020d7e2688b122bfb54c724ac9ee169f83f66d2fe90abeb95e8' +
      'e1290e7e177152a4de3d944cf7d4883114a20ed0f78e70e25ef0f' +
      '60f06b858e6af42a2f276ede95bbc6bc9a9bbdda15bd663186a6f' +
      '40819a7af19e577bb2efa5e579a1f5ce8a0d4ca8b8f6';

rsaSign.update(rsaPubPem);
let rsaSignature = rsaSign.sign(rsaKeyPem, 'hex');
assert.strictEqual(rsaSignature, expectedSignature);

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
  const privateKey = fixtures.readSync('test_rsa_privkey_2.pem');
  const publicKey = fixtures.readSync('test_rsa_pubkey_2.pem');

  const input = 'I AM THE WALRUS';

  const signature =
      '79d59d34f56d0e94aa6a3e306882b52ed4191f07521f25f505a078dc2f89' +
      '396e0c8ac89e996fde5717f4cb89199d8fec249961fcb07b74cd3d2a4ffa' +
      '235417b69618e4bcd76b97e29975b7ce862299410e1b522a328e44ac9bb2' +
      '8195e0268da7eda23d9825ac43c724e86ceeee0d0d4465678652ccaf6501' +
      '0ddfb299bedeb1ad';

  const sign = crypto.createSign('SHA256');
  sign.update(input);

  const output = sign.sign(privateKey, 'hex');
  assert.strictEqual(signature, output);

  const verify = crypto.createVerify('SHA256');
  verify.update(input);

  assert.strictEqual(verify.verify(publicKey, signature, 'hex'), true);

  // Test the legacy signature algorithm name.
  const sign2 = crypto.createSign('RSA-SHA256');
  sign2.update(input);

  const output2 = sign2.sign(privateKey, 'hex');
  assert.strictEqual(signature, output2);

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
  // against
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
  // against
  const sign = crypto.createSign('SHA1');
  sign.update(input);
  const signOptions = { key: dsaKeyPemEncrypted, passphrase: 'password' };
  const signature = sign.sign(signOptions, 'hex');

  const verify = crypto.createVerify('SHA1');
  verify.update(input);

  assert.strictEqual(verify.verify(dsaPubPem, signature, 'hex'), true);
}
