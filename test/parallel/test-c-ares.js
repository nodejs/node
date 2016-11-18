'use strict';
const common = require('../common');
const assert = require('assert');

const dns = require('dns');


// Try resolution without callback

dns.lookup(null, common.mustCall((error, result, addressType) => {
  assert.ifError(error);
  assert.strictEqual(null, result);
  assert.strictEqual(4, addressType);
}));

dns.lookup('127.0.0.1', common.mustCall((error, result, addressType) => {
  assert.ifError(error);
  assert.strictEqual('127.0.0.1', result);
  assert.strictEqual(4, addressType);
}));

dns.lookup('::1', common.mustCall((error, result, addressType) => {
  assert.ifError(error);
  assert.strictEqual('::1', result);
  assert.strictEqual(6, addressType);
}));

// Try calling resolve with an unsupported type.
assert.throws(() => dns.resolve('www.google.com', 'HI'), /Unknown type/);

// Try calling resolve with an unsupported type that's an object key
assert.throws(() => dns.resolve('www.google.com', 'toString'), /Unknown type/);

// Windows doesn't usually have an entry for localhost 127.0.0.1 in
// C:\Windows\System32\drivers\etc\hosts
// so we disable this test on Windows.
if (!common.isWindows) {
  dns.reverse('127.0.0.1', common.mustCall(function(error, domains) {
    assert.ifError(error);
    assert.ok(Array.isArray(domains));
  }));
}
