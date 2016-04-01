'use strict';
require('../common');
var assert = require('assert');

var dns = require('dns');

var existing = dns.getServers();
assert(existing.length);

function noop() {}

var goog = [
  '8.8.8.8',
  '8.8.4.4',
];
assert.doesNotThrow(function() { dns.setServers(goog); });
assert.deepEqual(dns.getServers(), goog);
assert.throws(function() { dns.setServers(['foobar']); });
assert.deepEqual(dns.getServers(), goog);

var goog6 = [
  '2001:4860:4860::8888',
  '2001:4860:4860::8844',
];
assert.doesNotThrow(function() { dns.setServers(goog6); });
assert.deepEqual(dns.getServers(), goog6);

goog6.push('4.4.4.4');
dns.setServers(goog6);
assert.deepEqual(dns.getServers(), goog6);

var ports = [
  '4.4.4.4:53',
  '[2001:4860:4860::8888]:53',
];
var portsExpected = [
  '4.4.4.4',
  '2001:4860:4860::8888',
];
dns.setServers(ports);
assert.deepEqual(dns.getServers(), portsExpected);

assert.doesNotThrow(function() { dns.setServers([]); });
assert.deepEqual(dns.getServers(), []);

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
