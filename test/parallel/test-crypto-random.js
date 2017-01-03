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
  return;
}
const assert = require('assert');
const crypto = require('crypto');

crypto.DEFAULT_ENCODING = 'buffer';

// bump, we register a lot of exit listeners
process.setMaxListeners(256);

const expectedErrorRegexp = /^TypeError: size must be a number >= 0$/;
[crypto.randomBytes, crypto.pseudoRandomBytes].forEach(function(f) {
  [-1, undefined, null, false, true, {}, []].forEach(function(value) {
    assert.throws(function() { f(value); }, expectedErrorRegexp);
    assert.throws(function() { f(value, function() {}); }, expectedErrorRegexp);
  });

  [0, 1, 2, 4, 16, 256, 1024].forEach(function(len) {
    f(len, common.mustCall(function(ex, buf) {
      assert.strictEqual(ex, null);
      assert.strictEqual(buf.length, len);
      assert.ok(Buffer.isBuffer(buf));
    }));
  });
});

// #5126, "FATAL ERROR: v8::Object::SetIndexedPropertiesToExternalArrayData()
// length exceeds max acceptable value"
assert.throws(function() {
  crypto.randomBytes((-1 >>> 0) + 1);
}, TypeError);
