'use strict';
var common = require('../common');
var assert = require('assert');
var fs = require('fs');

if (!common.hasCrypto) {
  common.skip('missing crypto');
  return;
}
var crypto = require('crypto');

// Test certificates
var certPem = fs.readFileSync(common.fixturesDir + '/test_cert.pem', 'ascii');
var keyPem = fs.readFileSync(common.fixturesDir + '/test_key.pem', 'ascii');

// Test signing and verifying
{
  const s1 = crypto.createSign('RSA-SHA1')
                   .update('Test123')
                   .sign(keyPem, 'base64');
  let s1stream = crypto.createSign('RSA-SHA1');
  s1stream.end('Test123');
  s1stream = s1stream.sign(keyPem, 'base64');
  assert.equal(s1, s1stream, 'Stream produces same output');

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
  assert.equal(s2, s2stream, 'Stream produces same output');

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
