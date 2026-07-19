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

  await assert.rejects(dnsPromises.lookup(null), {
    code: 'ERR_INVALID_ARG_VALUE',
  });

  res = await dnsPromises.lookup('127.0.0.1');
  assert.strictEqual(res.address, '127.0.0.1');
  assert.strictEqual(res.family, 4);

  res = await dnsPromises.lookup('::1');
  assert.strictEqual(res.address, '::1');
  assert.strictEqual(res.family, 6);
})().then(common.mustCall());

// Try resolution without hostname.
assert.throws(() => dns.lookup(null, common.mustNotCall()), {
  code: 'ERR_INVALID_ARG_VALUE',
});

dns.lookup('127.0.0.1', common.mustSucceed((result, addressType) => {
  assert.strictEqual(result, '127.0.0.1');
  assert.strictEqual(addressType, 4);
}));

dns.lookup('::1', common.mustSucceed((result, addressType) => {
  assert.strictEqual(result, '::1');
  assert.strictEqual(addressType, 6);
}));

[
  // Try calling resolve with an unsupported type.
  'HI',
  // Try calling resolve with an unsupported type that's an object key
  'toString',
].forEach((val) => {
  const err = {
    code: 'ERR_INVALID_ARG_VALUE',
    name: 'TypeError',
    message: `The argument 'rrtype' is invalid. Received '${val}'`,
  };

  assert.throws(
    () => dns.resolve('www.google.com', val),
    err
  );

  assert.throws(() => dnsPromises.resolve('www.google.com', val), err);
});

// Windows doesn't usually have an entry for localhost 127.0.0.1 in
// C:\Windows\System32\drivers\etc\hosts
// so we disable this test on Windows.
// IBMi reports `ENOTFOUND` when get hostname by address 127.0.0.1
if (!common.isWindows && !common.isIBMi) {
  dns.reverse('127.0.0.1', common.mustSucceed((domains) => {
    assert.ok(Array.isArray(domains));
  }));

  (async function() {
    assert.ok(Array.isArray(await dnsPromises.reverse('127.0.0.1')));
  })().then(common.mustCall());
}
