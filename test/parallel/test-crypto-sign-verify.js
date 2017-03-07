'use strict';
const common = require('../common');
const assert = require('assert');
const fs = require('fs');

if (!common.hasCrypto) {
  common.skip('missing crypto');
  return;
}
const crypto = require('crypto');

// Test certificates
const certPem = fs.readFileSync(common.fixturesDir + '/test_cert.pem', 'ascii');
const keyPem = fs.readFileSync(common.fixturesDir + '/test_key.pem', 'ascii');

// Test signing and verifying
{
  const s1 = crypto.createSign('RSA-SHA1')
                   .update('Test123')
                   .sign(keyPem, 'base64');
  let s1stream = crypto.createSign('RSA-SHA1');
  s1stream.end('Test123');
  s1stream = s1stream.sign(keyPem, 'base64');
  assert.strictEqual(s1, s1stream, 'Stream produces same output');

  const verified = crypto.createVerify('RSA-SHA1')
                         .update('Test')
                         .update('123')
                         .verify(certPem, s1, 'base64');
  assert.strictEqual(verified, true, 'sign and verify (base 64)');
}

{
  const s2 = crypto.createSign('RSA-SHA256')
                   .update('Test123')
                   .sign(keyPem, 'latin1');
  let s2stream = crypto.createSign('RSA-SHA256');
  s2stream.end('Test123');
  s2stream = s2stream.sign(keyPem, 'latin1');
  assert.strictEqual(s2, s2stream, 'Stream produces same output');

  let verified = crypto.createVerify('RSA-SHA256')
                       .update('Test')
                       .update('123')
                       .verify(certPem, s2, 'latin1');
  assert.strictEqual(verified, true, 'sign and verify (latin1)');

  const verStream = crypto.createVerify('RSA-SHA256');
  verStream.write('Tes');
  verStream.write('t12');
  verStream.end('3');
  verified = verStream.verify(certPem, s2, 'latin1');
  assert.strictEqual(verified, true, 'sign and verify (stream)');
}

{
  const s3 = crypto.createSign('RSA-SHA1')
                   .update('Test123')
                   .sign(keyPem, 'buffer');
  let verified = crypto.createVerify('RSA-SHA1')
                       .update('Test')
                       .update('123')
                       .verify(certPem, s3);
  assert.strictEqual(verified, true, 'sign and verify (buffer)');

  const verStream = crypto.createVerify('RSA-SHA1');
  verStream.write('Tes');
  verStream.write('t12');
  verStream.end('3');
  verified = verStream.verify(certPem, s3);
  assert.strictEqual(verified, true, 'sign and verify (stream)');
}

{
  [ 'RSA-SHA1', 'RSA-SHA256' ].forEach((algo) => {
    [ null, -2, -1, 0, 16, 32, 64 ].forEach((saltLength) => {
      let verified;

      // Test sign and verify with the given parameters
      const s4 = crypto.createSign(algo)
                       .update('Test123')
                       .sign({
                         key: keyPem,
                         padding: crypto.constants.RSA_PKCS1_PSS_PADDING,
                         saltLength
                       });
      verified = crypto.createVerify(algo)
                       .update('Test')
                       .update('123')
                       .verify({
                         key: certPem,
                         padding: crypto.constants.RSA_PKCS1_PSS_PADDING,
                         saltLength
                       }, s4);
      assert.strictEqual(verified, true, 'sign and verify (buffer, PSS)');

      // Setting the salt length to RSA_PSS_SALTLEN_AUTO should always work for
      // verification
      verified = crypto.createVerify(algo)
                       .update('Test123')
                       .verify({
                         key: certPem,
                         padding: crypto.constants.RSA_PKCS1_PSS_PADDING,
                         saltLength: crypto.constants.RSA_PSS_SALTLEN_AUTO
                       }, s4);
      assert.strictEqual(verified, true, 'sign and verify (buffer, PSS)');
    });
  });
}

{
  assert.throws(() => {
    crypto.createSign('RSA-SHA1')
      .update('Test123')
      .sign({
        key: keyPem,
        padding: crypto.constants.RSA_PKCS1_OAEP_PADDING
      });
  }, /^Error: Padding must be RSA_PKCS1_PADDING or RSA_PKCS1_PSS_PADDING$/);
}

// Test throws exception when key options is null
{
  assert.throws(() => {
    crypto.createSign('RSA-SHA1').update('Test123').sign(null, 'base64');
  }, /^Error: No key provided to sign$/);
}
