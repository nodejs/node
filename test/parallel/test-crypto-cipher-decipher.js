'use strict';
var common = require('../common');
var assert = require('assert');

if (!common.hasCrypto) {
  common.skip('missing crypto');
  return;
}
if (common.hasFipsCrypto) {
  common.skip('not supported in FIPS mode');
  return;
}
var crypto = require('crypto');

function testCipher1(key) {
  // Test encryption and decryption
  var plaintext = 'Keep this a secret? No! Tell everyone about node.js!';
  var cipher = crypto.createCipher('aes192', key);

  // encrypt plaintext which is in utf8 format
  // to a ciphertext which will be in hex
  var ciph = cipher.update(plaintext, 'utf8', 'hex');
  // Only use binary or hex, not base64.
  ciph += cipher.final('hex');

  var decipher = crypto.createDecipher('aes192', key);
  var txt = decipher.update(ciph, 'hex', 'utf8');
  txt += decipher.final('utf8');

  assert.equal(txt, plaintext, 'encryption and decryption');

  // streaming cipher interface
  // NB: In real life, it's not guaranteed that you can get all of it
  // in a single read() like this.  But in this case, we know it's
  // quite small, so there's no harm.
  var cStream = crypto.createCipher('aes192', key);
  cStream.end(plaintext);
  ciph = cStream.read();

  var dStream = crypto.createDecipher('aes192', key);
  dStream.end(ciph);
  txt = dStream.read().toString('utf8');

  assert.equal(txt, plaintext, 'encryption and decryption with streams');
}


function testCipher2(key) {
  // encryption and decryption with Base64
  // reported in https://github.com/joyent/node/issues/738
  var plaintext =
      '32|RmVZZkFUVmpRRkp0TmJaUm56ZU9qcnJkaXNNWVNpTTU*|iXmckfRWZBGWWELw' +
      'eCBsThSsfUHLeRe0KCsK8ooHgxie0zOINpXxfZi/oNG7uq9JWFVCk70gfzQH8ZUJ' +
      'jAfaFg**';
  var cipher = crypto.createCipher('aes256', key);

  // encrypt plaintext which is in utf8 format
  // to a ciphertext which will be in Base64
  var ciph = cipher.update(plaintext, 'utf8', 'base64');
  ciph += cipher.final('base64');

  var decipher = crypto.createDecipher('aes256', key);
  var txt = decipher.update(ciph, 'base64', 'utf8');
  txt += decipher.final('utf8');

  assert.equal(txt, plaintext, 'encryption and decryption with Base64');
}

testCipher1('MySecretKey123');
testCipher1(Buffer.from('MySecretKey123'));

testCipher2('0123456789abcdef');
testCipher2(Buffer.from('0123456789abcdef'));

// Base64 padding regression test, see #4837.
{
  const c = crypto.createCipher('aes-256-cbc', 'secret');
  const s = c.update('test', 'utf8', 'base64') + c.final('base64');
  assert.strictEqual(s, '375oxUQCIocvxmC5At+rvA==');
}

// Calling Cipher.final() or Decipher.final() twice should error but
// not assert. See #4886.
{
  const c = crypto.createCipher('aes-256-cbc', 'secret');
  try { c.final('xxx'); } catch (e) { /* Ignore. */ }
  try { c.final('xxx'); } catch (e) { /* Ignore. */ }
  try { c.final('xxx'); } catch (e) { /* Ignore. */ }
  const d = crypto.createDecipher('aes-256-cbc', 'secret');
  try { d.final('xxx'); } catch (e) { /* Ignore. */ }
  try { d.final('xxx'); } catch (e) { /* Ignore. */ }
  try { d.final('xxx'); } catch (e) { /* Ignore. */ }
}

// Regression test for #5482: string to Cipher#update() should not assert.
{
  const c = crypto.createCipher('aes192', '0123456789abcdef');
  c.update('update');
  c.final();
}

// #5655 regression tests, 'utf-8' and 'utf8' are identical.
{
  let c = crypto.createCipher('aes192', '0123456789abcdef');
  c.update('update', '');  // Defaults to "utf8".
  c.final('utf-8');  // Should not throw.

  c = crypto.createCipher('aes192', '0123456789abcdef');
  c.update('update', 'utf8');
  c.final('utf-8');  // Should not throw.

  c = crypto.createCipher('aes192', '0123456789abcdef');
  c.update('update', 'utf-8');
  c.final('utf8');  // Should not throw.
}
