// Copyright Joyent, Inc. and other Node contributors.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to permit
// persons to whom the Software is furnished to do so, subject to the
// following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
// USE OR OTHER DEALINGS IN THE SOFTWARE.




var common = require('../common');
var assert = require('assert');

try {
  var crypto = require('crypto');
} catch (e) {
  console.log('Not compiled with OPENSSL support.');
  process.exit();
}

crypto.DEFAULT_ENCODING = 'buffer';

//
// Test authenticated encryption modes.
//
// !NEVER USE STATIC IVs IN REAL LIFE!
//

var TEST_CASES = [
  { algo: 'aes-128-gcm', key: 'ipxp9a6i1Mb4USb4',
    iv: 'X6sIq117H0vR', plain: 'Hello World!',
    ct: '4BE13896F64DFA2C2D0F2C76',
    tag: '272B422F62EB545EAA15B5FF84092447', tampered: false },
  { algo: 'aes-128-gcm', key: 'ipxp9a6i1Mb4USb4',
    iv: 'X6sIq117H0vR', plain: 'Hello World!',
    ct: '4BE13896F64DFA2C2D0F2C76', aad: '000000FF',
    tag: 'BA2479F66275665A88CB7B15F43EB005', tampered: false },
  { algo: 'aes-128-gcm', key: 'ipxp9a6i1Mb4USb4',
    iv: 'X6sIq117H0vR', plain: 'Hello World!',
    ct: '4BE13596F64DFA2C2D0FAC76',
    tag: '272B422F62EB545EAA15B5FF84092447', tampered: true },
  { algo: 'aes-256-gcm', key: '3zTvzr3p67VC61jmV54rIYu1545x4TlY',
    iv: '60iP0h6vJoEa', plain: 'Hello node.js world!',
    ct: '58E62CFE7B1D274111A82267EBB93866E72B6C2A',
    tag: '9BB44F663BADABACAE9720881FB1EC7A', tampered: false },
  { algo: 'aes-256-gcm', key: '3zTvzr3p67VC61jmV54rIYu1545x4TlY',
    iv: '60iP0h6vJoEa', plain: 'Hello node.js world!',
    ct: '58E62CFF7B1D274011A82267EBB93866E72B6C2B',
    tag: '9BB44F663BADABACAE9720881FB1EC7A', tampered: true },
];

var ciphers = crypto.getCiphers();

for (var i in TEST_CASES) {
  var test = TEST_CASES[i];

  if (ciphers.indexOf(test.algo) == -1) {
    console.log('skipping unsupported ' + test.algo + ' test');
    continue;
  }

  (function() {
    var encrypt = crypto.createCipheriv(test.algo, test.key, test.iv);
    if (test.aad)
      encrypt.setAAD(new Buffer(test.aad, 'hex'));
    var hex = encrypt.update(test.plain, 'ascii', 'hex');
    hex += encrypt.final('hex');
    var auth_tag = encrypt.getAuthTag();
    // only test basic encryption run if output is marked as tampered.
    if (!test.tampered) {
      assert.equal(hex.toUpperCase(), test.ct);
      assert.equal(auth_tag.toString('hex').toUpperCase(), test.tag);
    }
  })();

  (function() {
    var decrypt = crypto.createDecipheriv(test.algo, test.key, test.iv);
    decrypt.setAuthTag(new Buffer(test.tag, 'hex'));
    if (test.aad)
      decrypt.setAAD(new Buffer(test.aad, 'hex'));
    var msg = decrypt.update(test.ct, 'hex', 'ascii');
    if (!test.tampered) {
      msg += decrypt.final('ascii');
      assert.equal(msg, test.plain);
    } else {
      // assert that final throws if input data could not be verified!
      assert.throws(function() { decrypt.final('ascii'); }, / auth/);
    }
  })();

  // after normal operation, test some incorrect ways of calling the API:
  // it's most certainly enough to run these tests with one algorithm only.

  if (i > 0) {
    continue;
  }

  (function() {
    // non-authenticating mode:
    var encrypt = crypto.createCipheriv('aes-128-cbc',
      'ipxp9a6i1Mb4USb4', '6fKjEjR3Vl30EUYC');
    encrypt.update('blah', 'ascii');
    encrypt.final();
    assert.throws(function() { encrypt.getAuthTag(); }, / state/);
    assert.throws(function() {
      encrypt.setAAD(new Buffer('123', 'ascii')); }, / state/);
  })();

  (function() {
    // trying to get tag before inputting all data:
    var encrypt = crypto.createCipheriv(test.algo, test.key, test.iv);
    encrypt.update('blah', 'ascii');
    assert.throws(function() { encrypt.getAuthTag(); }, / state/);
  })();

  (function() {
    // trying to set tag on encryption object:
    var encrypt = crypto.createCipheriv(test.algo, test.key, test.iv);
    assert.throws(function() {
      encrypt.setAuthTag(new Buffer(test.tag, 'hex')); }, / state/);
  })();

  (function() {
    // trying to read tag from decryption object:
    var decrypt = crypto.createDecipheriv(test.algo, test.key, test.iv);
    assert.throws(function() { decrypt.getAuthTag(); }, / state/);
  })();
}
