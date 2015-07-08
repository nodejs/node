// Flags: --experimental-workers
'use strict';

var assert = require('assert');
var util = require('util');
var Worker = require('worker');
var common = require('../common');
var checks = 0;
// When using OpenSSL this test should put the locking_callback to good use
var tests = [
  'test/parallel/test-crypto-authenticated.js',
  'test/parallel/test-crypto-binary-default.js',
  'test/parallel/test-crypto-certificate.js',
  'test/parallel/test-crypto-cipher-decipher.js',
  'test/parallel/test-crypto-dh.js',
  'test/parallel/test-crypto-dh-odd-key.js',
  'test/parallel/test-crypto-domain.js',
  'test/parallel/test-crypto-domains.js',
  'test/parallel/test-crypto-ecb.js',
  'test/parallel/test-crypto-from-binary.js',
  'test/parallel/test-crypto-hash.js',
  'test/parallel/test-crypto-hash-stream-pipe.js',
  'test/parallel/test-crypto-hmac.js',
  'test/parallel/test-crypto.js',
  'test/parallel/test-crypto-padding-aes256.js',
  'test/parallel/test-crypto-padding.js',
  'test/parallel/test-crypto-pbkdf2.js',
  'test/parallel/test-crypto-random.js',
  'test/parallel/test-crypto-rsa-dsa.js',
  'test/parallel/test-crypto-sign-verify.js',
  'test/parallel/test-crypto-stream.js'
];

var parallelism = 4;
var testsPerThread = Math.ceil(tests.length / parallelism);
for (var i = 0; i < parallelism; ++i) {
  var shareOfTests = tests.slice(i * testsPerThread, (i + 1) * testsPerThread);
  var cur = Promise.resolve();
  shareOfTests.forEach(function(testFile) {
    cur = cur.then(function() {
      return common.runTestInsideWorker(testFile);
    });
  });
}
