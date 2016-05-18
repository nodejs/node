'use strict';
require('../common');
const assert = require('assert');

const dns = require('dns');

var existing = dns.getServers();
assert(existing.length);

function noop() {}

var goog = [
  '8.8.8.8',
  '8.8.4.4',
];
assert.doesNotThrow(function() { dns.setServers(goog); });
assert.deepStrictEqual(dns.getServers(), goog);
assert.throws(function() { dns.setServers(['foobar']); });
assert.deepStrictEqual(dns.getServers(), goog);

var goog6 = [
  '2001:4860:4860::8888',
  '2001:4860:4860::8844',
];
assert.doesNotThrow(function() { dns.setServers(goog6); });
assert.deepStrictEqual(dns.getServers(), goog6);

goog6.push('4.4.4.4');
dns.setServers(goog6);
assert.deepStrictEqual(dns.getServers(), goog6);

var ports = [
  '4.4.4.4:53',
  '[2001:4860:4860::8888]:53',
];
var portsExpected = [
  '4.4.4.4',
  '2001:4860:4860::8888',
];
dns.setServers(ports);
assert.deepStrictEqual(dns.getServers(), portsExpected);

assert.doesNotThrow(function() { dns.setServers([]); });
assert.deepStrictEqual(dns.getServers(), []);

assert.throws(function() {
  dns.resolve('test.com', [], noop);
}, function(err) {
  return !(err instanceof TypeError);
}, 'Unexpected error');

// dns.lookup should accept falsey and string values
assert.throws(function() {
  dns.lookup({}, noop);
}, 'invalid arguments: hostname must be a string or falsey');

assert.throws(function() {
  dns.lookup([], noop);
}, 'invalid arguments: hostname must be a string or falsey');

assert.throws(function() {
  dns.lookup(true, noop);
}, 'invalid arguments: hostname must be a string or falsey');

assert.throws(function() {
  dns.lookup(1, noop);
}, 'invalid arguments: hostname must be a string or falsey');

assert.throws(function() {
  dns.lookup(noop, noop);
}, 'invalid arguments: hostname must be a string or falsey');

assert.doesNotThrow(function() {
  dns.lookup('', noop);
});

assert.doesNotThrow(function() {
  dns.lookup(null, noop);
});

assert.doesNotThrow(function() {
  dns.lookup(undefined, noop);
});

assert.doesNotThrow(function() {
  dns.lookup(0, noop);
});

assert.doesNotThrow(function() {
  dns.lookup(NaN, noop);
});

/*
 * Make sure that dns.lookup throws if hints does not represent a valid flag.
 * (dns.V4MAPPED | dns.ADDRCONFIG) + 1 is invalid because:
 * - it's different from dns.V4MAPPED and dns.ADDRCONFIG.
 * - it's different from them bitwise ored.
 * - it's different from 0.
 * - it's an odd number different than 1, and thus is invalid, because
 * flags are either === 1 or even.
 */
assert.throws(function() {
  dns.lookup('www.google.com', { hints: (dns.V4MAPPED | dns.ADDRCONFIG) + 1 },
    noop);
});

assert.throws(function() {
  dns.lookup('www.google.com');
}, 'invalid arguments: callback must be passed');

assert.throws(function() {
  dns.lookup('www.google.com', 4);
}, 'invalid arguments: callback must be passed');

assert.doesNotThrow(function() {
  dns.lookup('www.google.com', 6, noop);
});

assert.doesNotThrow(function() {
  dns.lookup('www.google.com', {}, noop);
});

assert.doesNotThrow(function() {
  dns.lookup('', {
    family: 4,
    hints: 0
  }, noop);
});

assert.doesNotThrow(function() {
  dns.lookup('', {
    family: 6,
    hints: dns.ADDRCONFIG
  }, noop);
});

assert.doesNotThrow(function() {
  dns.lookup('', {
    hints: dns.V4MAPPED
  }, noop);
});

assert.doesNotThrow(function() {
  dns.lookup('', {
    hints: dns.ADDRCONFIG | dns.V4MAPPED
  }, noop);
});

assert.throws(function() {
  dns.lookupService('0.0.0.0');
}, /Invalid arguments/);

assert.throws(function() {
  dns.lookupService('fasdfdsaf', 0, noop);
}, /"host" argument needs to be a valid IP address/);

assert.doesNotThrow(function() {
  dns.lookupService('0.0.0.0', '0', noop);
});

assert.doesNotThrow(function() {
  dns.lookupService('0.0.0.0', 0, noop);
});

assert.throws(function() {
  dns.lookupService('0.0.0.0', null, noop);
}, /"port" should be >= 0 and < 65536, got "null"/);

assert.throws(function() {
  dns.lookupService('0.0.0.0', undefined, noop);
}, /"port" should be >= 0 and < 65536, got "undefined"/);

assert.throws(function() {
  dns.lookupService('0.0.0.0', 65538, noop);
}, /"port" should be >= 0 and < 65536, got "65538"/);

assert.throws(function() {
  dns.lookupService('0.0.0.0', 'test', noop);
}, /"port" should be >= 0 and < 65536, got "test"/);
