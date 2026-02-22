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
  common.skip('missing crypto');

const assert = require('assert');
const crypto = require('crypto');

const stream = require('stream');
const s = new stream.PassThrough();
const h = crypto.createHash('sha512');
const expect = 'fba055c6fd0c5b6645407749ed7a8b41' +
               'b8f629f2163c3ca3701d864adabda1f8' +
               '93c37bf82b22fdd151ba8e357f611da4' +
               '88a74b6a5525dd9b69554c6ce5138ad7';

s.pipe(h).on('data', common.mustCall(function(c) {
  assert.strictEqual(c, expect);
  // Calling digest() after piping into a stream with SHA3 should not cause
  // a segmentation fault, see https://github.com/nodejs/node/issues/28245.
  assert.strictEqual(h.digest('hex'), expect);
})).setEncoding('hex');

s.end('aoeu');
