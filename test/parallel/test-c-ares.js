'use strict';
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

// Try calling resolve with an unsupported type that's an object key
assert.throws(function() {
  dns.resolve('www.google.com', 'toString');
}, /Unknown type/);

// Windows doesn't usually have an entry for localhost 127.0.0.1 in
// C:\Windows\System32\drivers\etc\hosts
// so we disable this test on Windows.
if (!common.isWindows) {
  dns.reverse('127.0.0.1', function(error, domains) {
    if (error) throw error;
    assert.ok(Array.isArray(domains));
  });
}
