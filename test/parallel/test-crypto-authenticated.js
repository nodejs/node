'use strict';
var common = require('../common');
var assert = require('assert');

if (!common.hasCrypto) {
  console.log('1..0 # Skipped: missing crypto');
  process.exit();
}
var crypto = require('crypto');

crypto.DEFAULT_ENCODING = 'buffer';

//
// Test authenticated encryption modes.
//
// !NEVER USE STATIC IVs IN REAL LIFE!
//

var TEST_CASES = [
  { algo: 'aes-128-gcm',
    key: '6970787039613669314d623455536234',
    iv: '583673497131313748307652', plain: 'Hello World!',
    ct: '4BE13896F64DFA2C2D0F2C76',
    tag: '272B422F62EB545EAA15B5FF84092447', tampered: false },
  { algo: 'aes-128-gcm',
    key: '6970787039613669314d623455536234',
    iv: '583673497131313748307652', plain: 'Hello World!',
    ct: '4BE13896F64DFA2C2D0F2C76', aad: '000000FF',
    tag: 'BA2479F66275665A88CB7B15F43EB005', tampered: false },
  { algo: 'aes-128-gcm',
    key: '6970787039613669314d623455536234',
    iv: '583673497131313748307652', plain: 'Hello World!',
    ct: '4BE13596F64DFA2C2D0FAC76',
    tag: '272B422F62EB545EAA15B5FF84092447', tampered: true },
  { algo: 'aes-256-gcm',
    key: '337a54767a7233703637564336316a6d56353472495975313534357834546c59',
    iv: '36306950306836764a6f4561', plain: 'Hello node.js world!',
    ct: '58E62CFE7B1D274111A82267EBB93866E72B6C2A',
    tag: '9BB44F663BADABACAE9720881FB1EC7A', tampered: false },
  { algo: 'aes-256-gcm',
    key: '337a54767a7233703637564336316a6d56353472495975313534357834546c59',
    iv: '36306950306836764a6f4561', plain: 'Hello node.js world!',
    ct: '58E62CFF7B1D274011A82267EBB93866E72B6C2B',
    tag: '9BB44F663BADABACAE9720881FB1EC7A', tampered: true },
  { algo: 'aes-192-gcm',
    key: '1ed2233fa2223ef5d7df08546049406c7305220bca40d4c9',
    iv: '0e1791e9db3bd21a9122c416', plain: 'Hello node.js world!',
    password: 'very bad password', aad: '63616c76696e',
    ct: 'DDA53A4059AA17B88756984995F7BBA3C636CC44',
    tag: 'D2A35E5C611E5E3D2258360241C5B045', tampered: false }
];

var ciphers = crypto.getCiphers();

for (var i in TEST_CASES) {
  var test = TEST_CASES[i];

  if (ciphers.indexOf(test.algo) == -1) {
    console.log('skipping unsupported ' + test.algo + ' test');
    continue;
  }

  (function() {
    var encrypt = crypto.createCipheriv(test.algo,
      new Buffer(test.key, 'hex'), new Buffer(test.iv, 'hex'));
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
    var decrypt = crypto.createDecipheriv(test.algo,
      new Buffer(test.key, 'hex'), new Buffer(test.iv, 'hex'));
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

  (function() {
    if (!test.password) return;
    var encrypt = crypto.createCipher(test.algo, test.password);
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
    if (!test.password) return;
    var decrypt = crypto.createDecipher(test.algo, test.password);
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
    var encrypt = crypto.createCipheriv(test.algo,
      new Buffer(test.key, 'hex'), new Buffer(test.iv, 'hex'));
    encrypt.update('blah', 'ascii');
    assert.throws(function() { encrypt.getAuthTag(); }, / state/);
  })();

  (function() {
    // trying to set tag on encryption object:
    var encrypt = crypto.createCipheriv(test.algo,
      new Buffer(test.key, 'hex'), new Buffer(test.iv, 'hex'));
    assert.throws(function() {
      encrypt.setAuthTag(new Buffer(test.tag, 'hex')); }, / state/);
  })();

  (function() {
    // trying to read tag from decryption object:
    var decrypt = crypto.createDecipheriv(test.algo,
      new Buffer(test.key, 'hex'), new Buffer(test.iv, 'hex'));
    assert.throws(function() { decrypt.getAuthTag(); }, / state/);
  })();
}
