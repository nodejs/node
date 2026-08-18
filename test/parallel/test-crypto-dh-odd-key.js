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
  common.skip('missing crypto');
}

const assert = require('assert');
const crypto = require('crypto');
const { hasOpenSSL, hasFIPS } = require('../common/crypto');

function test() {
  const odd = Buffer.alloc(39, 'A');

  const size = hasFIPS(3) ? 2048 : (hasOpenSSL(3) ? 1024 : 32);
  const c = crypto.createDiffieHellman(size);
  c.setPrivateKey(odd);
  c.generateKeys();
}

if (hasFIPS(3)) {
  test();
  assert.throws(() => crypto.createDiffieHellman(1024), {
    code: 'ERR_INVALID_ARG_VALUE',
  });
} else if (crypto.getFips() !== 1) {
  test();
} else {
  assert.throws(test, /key size too small/);
}
