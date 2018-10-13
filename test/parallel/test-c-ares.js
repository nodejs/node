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
const assert = require('assert');

const dns = require('dns');
const dnsPromises = dns.promises;

(async function() {
  let res;

  res = await dnsPromises.lookup(null);
  assert.strictEqual(res.address, null);
  assert.strictEqual(res.family, 4);

  res = await dnsPromises.lookup('127.0.0.1');
  assert.strictEqual(res.address, '127.0.0.1');
  assert.strictEqual(res.family, 4);

  res = await dnsPromises.lookup('::1');
  assert.strictEqual(res.address, '::1');
  assert.strictEqual(res.family, 6);
})();

// Try resolution without callback

dns.lookup(null, common.mustCall((error, result, addressType) => {
  assert.ifError(error);
  assert.strictEqual(result, null);
  assert.strictEqual(addressType, 4);
}));

dns.lookup('127.0.0.1', common.mustCall((error, result, addressType) => {
  assert.ifError(error);
  assert.strictEqual(result, '127.0.0.1');
  assert.strictEqual(addressType, 4);
}));

dns.lookup('::1', common.mustCall((error, result, addressType) => {
  assert.ifError(error);
  assert.strictEqual(result, '::1');
  assert.strictEqual(addressType, 6);
}));

[
  // Try calling resolve with an unsupported type.
  'HI',
  // Try calling resolve with an unsupported type that's an object key
  'toString'
].forEach((val) => {
  const err = {
    code: 'ERR_INVALID_OPT_VALUE',
    type: TypeError,
    message: `The value "${val}" is invalid for option "rrtype"`
  };

  common.expectsError(
    () => dns.resolve('www.google.com', val),
    err
  );

  common.expectsError(() => dnsPromises.resolve('www.google.com', val), err);
});

// Windows doesn't usually have an entry for localhost 127.0.0.1 in
// C:\Windows\System32\drivers\etc\hosts
// so we disable this test on Windows.
if (!common.isWindows) {
  dns.reverse('127.0.0.1', common.mustCall(function(error, domains) {
    assert.ifError(error);
    assert.ok(Array.isArray(domains));
  }));

  (async function() {
    assert.ok(Array.isArray(await dnsPromises.reverse('127.0.0.1')));
  })();
}
