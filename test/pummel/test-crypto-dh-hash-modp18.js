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

'use strict';
const common = require('../common');

if (!common.hasCrypto) {
  common.skip('node compiled without OpenSSL.');
}

if (process.config.variables.arm_version === '7') {
  common.skip('Too slow for armv7 bots');
}

const assert = require('assert');
const crypto = require('crypto');

const hashes = {
  modp18: 'a870b491bbbec9b131ae9878d07449d32e54f160'
};

for (const name in hashes) {
  const group = crypto.getDiffieHellman(name);
  const private_key = group.getPrime('hex');
  const hash1 = hashes[name];
  const hash2 = crypto.createHash('sha1')
                    .update(private_key.toUpperCase()).digest('hex');
  assert.strictEqual(hash1, hash2);
  assert.strictEqual(group.getGenerator('hex'), '02');
}
