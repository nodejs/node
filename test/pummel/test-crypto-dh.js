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

assert.throws(function() {
  crypto.getDiffieHellman('unknown-group');
});
assert.throws(function() {
  crypto.getDiffieHellman('modp1').setPrivateKey('');
});
assert.throws(function() {
  crypto.getDiffieHellman('modp1').setPublicKey('');
});

var hashes = {
  modp1 : "b4b330a6ffeacfbd861e7fe2135b4431",
  modp2 : "7c3c5cad8b9f378d88f1dd64a4b6413a",
  modp5 : "b1d2acc22c542e08669a5c5ae812694d",
  modp14 : "8d041538cecc1a7d915ba4b718f8ad20",
  modp15 : "dc3b93def24e078c4fbf92d5e14ba69b",
  modp16 : "a273487f46f699461f613b3878d9dfd9",
  modp17 : "dc76e09935310348c492de9bd82014d0",
  modp18 : "db08973bfd2371758a69db180871c993"
}

for (var name in hashes) {
  var group = crypto.getDiffieHellman(name);
  var private_key = group.getPrime('hex');
  var hash1 = hashes[name];
  var hash2 = crypto.createHash('md5').update(private_key.toUpperCase()).digest('hex');
  assert.equal(hash1, hash2);
  assert.equal(group.getGenerator('hex'), '02');
}

for (var name in hashes) {
  var group1 = crypto.getDiffieHellman(name);
  var group2 = crypto.getDiffieHellman(name);
  group1.generateKeys();
  group2.generateKeys();
  var key1 = group1.computeSecret(group2.getPublicKey());
  var key2 = group2.computeSecret(group1.getPublicKey());
  assert.deepEqual(key1, key2);
}
