'use strict';
var common = require('../common');
var assert = require('assert');
var fs = require('fs');
var constants = require('crypto').constants;

if (!common.hasCrypto) {
  common.skip('missing crypto');
  return;
}
var crypto = require('crypto');

// Test certificates
var certPem = fs.readFileSync(common.fixturesDir + '/test_cert.pem', 'ascii');
var keyPem = fs.readFileSync(common.fixturesDir + '/test_key.pem', 'ascii');
var rsaPubPem = fs.readFileSync(common.fixturesDir + '/test_rsa_pubkey.pem',
    'ascii');
var rsaKeyPem = fs.readFileSync(common.fixturesDir + '/test_rsa_privkey.pem',
    'ascii');
var rsaKeyPemEncrypted = fs.readFileSync(
  common.fixturesDir + '/test_rsa_privkey_encrypted.pem', 'ascii');
var dsaPubPem = fs.readFileSync(common.fixturesDir + '/test_dsa_pubkey.pem',
    'ascii');
var dsaKeyPem = fs.readFileSync(common.fixturesDir + '/test_dsa_privkey.pem',
    'ascii');
var dsaKeyPemEncrypted = fs.readFileSync(
  common.fixturesDir + '/test_dsa_privkey_encrypted.pem', 'ascii');

// Test RSA encryption/decryption
{
  const input = 'I AM THE WALRUS';
  const bufferToEncrypt = Buffer.from(input);

  let encryptedBuffer = crypto.publicEncrypt(rsaPubPem, bufferToEncrypt);

  let decryptedBuffer = crypto.privateDecrypt(rsaKeyPem, encryptedBuffer);
  assert.strictEqual(input, decryptedBuffer.toString());

  let decryptedBufferWithPassword = crypto.privateDecrypt({
    key: rsaKeyPemEncrypted,
    passphrase: 'password'
  }, encryptedBuffer);
  assert.strictEqual(input, decryptedBufferWithPassword.toString());

  encryptedBuffer = crypto.publicEncrypt({
    key: rsaKeyPemEncrypted,
    passphrase: 'password'
  }, bufferToEncrypt);

  decryptedBufferWithPassword = crypto.privateDecrypt({
    key: rsaKeyPemEncrypted,
    passphrase: 'password'
  }, encryptedBuffer);
  assert.strictEqual(input, decryptedBufferWithPassword.toString());

  encryptedBuffer = crypto.privateEncrypt({
    key: rsaKeyPemEncrypted,
    passphrase: Buffer.from('password')
  }, bufferToEncrypt);

  decryptedBufferWithPassword = crypto.publicDecrypt({
    key: rsaKeyPemEncrypted,
    passphrase: Buffer.from('password')
  }, encryptedBuffer);
  assert.strictEqual(input, decryptedBufferWithPassword.toString());

  encryptedBuffer = crypto.publicEncrypt(certPem, bufferToEncrypt);

  decryptedBuffer = crypto.privateDecrypt(keyPem, encryptedBuffer);
  assert.strictEqual(input, decryptedBuffer.toString());

  encryptedBuffer = crypto.publicEncrypt(keyPem, bufferToEncrypt);

  decryptedBuffer = crypto.privateDecrypt(keyPem, encryptedBuffer);
  assert.strictEqual(input, decryptedBuffer.toString());

  encryptedBuffer = crypto.privateEncrypt(keyPem, bufferToEncrypt);

  decryptedBuffer = crypto.publicDecrypt(keyPem, encryptedBuffer);
  assert.strictEqual(input, decryptedBuffer.toString());

  assert.throws(function() {
    crypto.privateDecrypt({
      key: rsaKeyPemEncrypted,
      passphrase: 'wrong'
    }, bufferToEncrypt);
  });

  assert.throws(function() {
    crypto.publicEncrypt({
      key: rsaKeyPemEncrypted,
      passphrase: 'wrong'
    }, encryptedBuffer);
  });

  encryptedBuffer = crypto.privateEncrypt({
    key: rsaKeyPemEncrypted,
    passphrase: Buffer.from('password')
  }, bufferToEncrypt);

  assert.throws(function() {
    crypto.publicDecrypt({
      key: rsaKeyPemEncrypted,
      passphrase: [].concat.apply([], Buffer.from('password'))
    }, encryptedBuffer);
  });
}

function test_rsa(padding) {
  var input = Buffer.allocUnsafe(padding === 'RSA_NO_PADDING' ? 1024 / 8 : 32);
  for (var i = 0; i < input.length; i++)
    input[i] = (i * 7 + 11) & 0xff;
  var bufferToEncrypt = Buffer.from(input);

  padding = constants[padding];

  var encryptedBuffer = crypto.publicEncrypt({
    key: rsaPubPem,
    padding: padding
  }, bufferToEncrypt);

  var decryptedBuffer = crypto.privateDecrypt({
    key: rsaKeyPem,
    padding: padding
  }, encryptedBuffer);
  assert.equal(input, decryptedBuffer.toString());
}

test_rsa('RSA_NO_PADDING');
test_rsa('RSA_PKCS1_PADDING');
test_rsa('RSA_PKCS1_OAEP_PADDING');

// Test RSA key signing/verification
var rsaSign = crypto.createSign('RSA-SHA1');
var rsaVerify = crypto.createVerify('RSA-SHA1');
assert.ok(rsaSign);
assert.ok(rsaVerify);

rsaSign.update(rsaPubPem);
var rsaSignature = rsaSign.sign(rsaKeyPem, 'hex');
assert.equal(rsaSignature,
             '5c50e3145c4e2497aadb0eabc83b342d0b0021ece0d4c4a064b7c' +
             '8f020d7e2688b122bfb54c724ac9ee169f83f66d2fe90abeb95e8' +
             'e1290e7e177152a4de3d944cf7d4883114a20ed0f78e70e25ef0f' +
             '60f06b858e6af42a2f276ede95bbc6bc9a9bbdda15bd663186a6f' +
             '40819a7af19e577bb2efa5e579a1f5ce8a0d4ca8b8f6');

rsaVerify.update(rsaPubPem);
assert.strictEqual(rsaVerify.verify(rsaPubPem, rsaSignature, 'hex'), true);

// Test RSA key signing/verification with encrypted key
rsaSign = crypto.createSign('RSA-SHA1');
rsaSign.update(rsaPubPem);
assert.doesNotThrow(function() {
  var signOptions = { key: rsaKeyPemEncrypted, passphrase: 'password' };
  rsaSignature = rsaSign.sign(signOptions, 'hex');
});
assert.equal(rsaSignature,
             '5c50e3145c4e2497aadb0eabc83b342d0b0021ece0d4c4a064b7c' +
             '8f020d7e2688b122bfb54c724ac9ee169f83f66d2fe90abeb95e8' +
             'e1290e7e177152a4de3d944cf7d4883114a20ed0f78e70e25ef0f' +
             '60f06b858e6af42a2f276ede95bbc6bc9a9bbdda15bd663186a6f' +
             '40819a7af19e577bb2efa5e579a1f5ce8a0d4ca8b8f6');

rsaVerify = crypto.createVerify('RSA-SHA1');
rsaVerify.update(rsaPubPem);
assert.strictEqual(rsaVerify.verify(rsaPubPem, rsaSignature, 'hex'), true);

rsaSign = crypto.createSign('RSA-SHA1');
rsaSign.update(rsaPubPem);
assert.throws(function() {
  var signOptions = { key: rsaKeyPemEncrypted, passphrase: 'wrong' };
  rsaSign.sign(signOptions, 'hex');
});

//
// Test RSA signing and verification
//
{
  const privateKey = fs.readFileSync(
      common.fixturesDir + '/test_rsa_privkey_2.pem');

  const publicKey = fs.readFileSync(
      common.fixturesDir + '/test_rsa_pubkey_2.pem');

  const input = 'I AM THE WALRUS';

  const signature =
      '79d59d34f56d0e94aa6a3e306882b52ed4191f07521f25f505a078dc2f89' +
      '396e0c8ac89e996fde5717f4cb89199d8fec249961fcb07b74cd3d2a4ffa' +
      '235417b69618e4bcd76b97e29975b7ce862299410e1b522a328e44ac9bb2' +
      '8195e0268da7eda23d9825ac43c724e86ceeee0d0d4465678652ccaf6501' +
      '0ddfb299bedeb1ad';

  const sign = crypto.createSign('RSA-SHA256');
  sign.update(input);

  const output = sign.sign(privateKey, 'hex');
  assert.strictEqual(output, signature);

  const verify = crypto.createVerify('RSA-SHA256');
  verify.update(input);

  assert.strictEqual(verify.verify(publicKey, signature, 'hex'), true);
}


//
// Test DSA signing and verification
//
{
  const input = 'I AM THE WALRUS';

  // DSA signatures vary across runs so there is no static string to verify
  // against
  const sign = crypto.createSign('DSS1');
  sign.update(input);
  const signature = sign.sign(dsaKeyPem, 'hex');

  const verify = crypto.createVerify('DSS1');
  verify.update(input);

  assert.strictEqual(verify.verify(dsaPubPem, signature, 'hex'), true);
}


//
// Test DSA signing and verification with encrypted key
//
const input = 'I AM THE WALRUS';

{
  const sign = crypto.createSign('DSS1');
  sign.update(input);
  assert.throws(function() {
    sign.sign({ key: dsaKeyPemEncrypted, passphrase: 'wrong' }, 'hex');
  });
}

{
  // DSA signatures vary across runs so there is no static string to verify
  // against
  const sign = crypto.createSign('DSS1');
  sign.update(input);

  let signature;
  assert.doesNotThrow(function() {
    const signOptions = { key: dsaKeyPemEncrypted, passphrase: 'password' };
    signature = sign.sign(signOptions, 'hex');
  });

  const verify = crypto.createVerify('DSS1');
  verify.update(input);

  assert.strictEqual(verify.verify(dsaPubPem, signature, 'hex'), true);
}
