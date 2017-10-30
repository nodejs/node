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

if (!common.hasCrypto)
  common.skip('node compiled without OpenSSL.');

const crypto = require('crypto');

// Pollution of global is intentional as part of test.
common.globalCheck = false;
// See https://github.com/nodejs/node/commit/d1eff9ab
global.domain = require('domain');

// should not throw a 'TypeError: undefined is not a function' exception
crypto.randomBytes(8);
crypto.randomBytes(8, common.mustCall());
const buf = Buffer.alloc(8);
crypto.randomFillSync(buf);
crypto.pseudoRandomBytes(8);
crypto.pseudoRandomBytes(8, common.mustCall());
crypto.pbkdf2('password', 'salt', 8, 8, 'sha1', common.mustCall());
