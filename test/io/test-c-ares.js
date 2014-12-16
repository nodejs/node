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

var dns = require('dns');


// Try resolution without callback

dns.lookup(null, function(error, result, addressType) {
  assert.equal(null, result);
  assert.equal(4, addressType);
});

dns.lookup('127.0.0.1', function(error, result, addressType) {
  assert.equal('127.0.0.1', result);
  assert.equal(4, addressType);
});

dns.lookup('::1', function(error, result, addressType) {
  assert.equal('::1', result);
  assert.equal(6, addressType);
});

// Try calling resolve with an unsupported type.
assert.throws(function() {
  dns.resolve('www.google.com', 'HI');
}, /Unknown type/);

// Windows doesn't usually have an entry for localhost 127.0.0.1 in
// C:\Windows\System32\drivers\etc\hosts
// so we disable this test on Windows.
if (process.platform != 'win32') {
  dns.resolve('127.0.0.1', 'PTR', function(error, domains) {
    if (error) throw error;
    assert.ok(Array.isArray(domains));
  });
}
