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

// bump, we register a lot of exit listeners
process.setMaxListeners(256);

[crypto.randomBytes,
  crypto.pseudoRandomBytes
].forEach(function(f) {
  [-1,
    undefined,
    null,
    false,
    true,
    {},
    []
  ].forEach(function(value) {
    assert.throws(function() { f(value); });
    assert.throws(function() { f(value, function() {}); });
  });

  [0, 1, 2, 4, 16, 256, 1024].forEach(function(len) {
    f(len, checkCall(function(ex, buf) {
      assert.equal(null, ex);
      assert.equal(len, buf.length);
      assert.ok(Buffer.isBuffer(buf));
    }));
  });
});

// assert that the callback is indeed called
function checkCall(cb, desc) {
  var called_ = false;

  process.on('exit', function() {
    assert.equal(true, called_, desc || ('callback not called: ' + cb));
  });

  return function() {
    return called_ = true, cb.apply(cb, Array.prototype.slice.call(arguments));
  };
}
