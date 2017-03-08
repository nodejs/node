'use strict';
require('../common');
const common = require('../common');
const assert = require('assert');

const dns = require('dns');

const existing = dns.getServers();
assert(existing.length > 0);

// Verify that setServers() handles arrays with holes and other oddities
assert.doesNotThrow(() => {
  const servers = [];

  servers[0] = '127.0.0.1';
  servers[2] = '0.0.0.0';
  dns.setServers(servers);
});

assert.doesNotThrow(() => {
  const servers = ['127.0.0.1', '192.168.1.1'];

  servers[3] = '127.1.0.1';
  servers[4] = '127.1.0.1';
  servers[5] = '127.1.1.1';

  Object.defineProperty(servers, 2, {
    enumerable: true,
    get: () => {
      servers.length = 3;
      return '0.0.0.0';
    }
  });

  dns.setServers(servers);
});

function noop() {}

const goog = [
  '8.8.8.8',
  '8.8.4.4',
];
assert.doesNotThrow(() => dns.setServers(goog));
assert.deepStrictEqual(dns.getServers(), goog);
assert.throws(() => dns.setServers(['foobar']),
              common.expectsError({code: 'ERR_INVALID_IP', type: Error}));
assert.deepStrictEqual(dns.getServers(), goog);

const goog6 = [
  '2001:4860:4860::8888',
  '2001:4860:4860::8844',
];
assert.doesNotThrow(() => dns.setServers(goog6));
assert.deepStrictEqual(dns.getServers(), goog6);

goog6.push('4.4.4.4');
dns.setServers(goog6);
assert.deepStrictEqual(dns.getServers(), goog6);

const ports = [
  '4.4.4.4:53',
  '[2001:4860:4860::8888]:53',
];
const portsExpected = [
  '4.4.4.4',
  '2001:4860:4860::8888',
];
dns.setServers(ports);
assert.deepStrictEqual(dns.getServers(), portsExpected);

assert.doesNotThrow(() => dns.setServers([]));
assert.deepStrictEqual(dns.getServers(), []);

assert.throws(() => dns.resolve('test.com', [], noop),
              common.expectsError({code: 'ERR_INVALID_ARG_TYPE', type: Error}));

// dns.lookup should accept falsey and string values
const errorReg =
   common.expectsError({code: 'ERR_INVALID_ARG_TYPE', type: TypeError});

assert.throws(() => dns.lookup({}, noop), errorReg);

assert.throws(() => dns.lookup([], noop), errorReg);

assert.throws(() => dns.lookup(true, noop), errorReg);

assert.throws(() => dns.lookup(1, noop), errorReg);

assert.throws(() => dns.lookup(noop, noop), errorReg);

assert.doesNotThrow(() => dns.lookup('', noop));

assert.doesNotThrow(() => dns.lookup(null, noop));

assert.doesNotThrow(() => dns.lookup(undefined, noop));

assert.doesNotThrow(() => dns.lookup(0, noop));

assert.doesNotThrow(() => dns.lookup(NaN, noop));

/*
 * Make sure that dns.lookup throws if hints does not represent a valid flag.
 * (dns.V4MAPPED | dns.ADDRCONFIG) + 1 is invalid because:
 * - it's different from dns.V4MAPPED and dns.ADDRCONFIG.
 * - it's different from them bitwise ored.
 * - it's different from 0.
 * - it's an odd number different than 1, and thus is invalid, because
 * flags are either === 1 or even.
 */
assert.throws(() => {
  dns.lookup('www.google.com', { hints: (dns.V4MAPPED | dns.ADDRCONFIG) + 1 },
             noop);
}, common.expectsError({code: 'ERR_INVALID_FLAG', type: Error}));

assert.throws(() => dns.lookup('www.google.com'),
              common.expectsError({code: 'ERR_INVALID_CALLBACK',
                                   type: TypeError}));

assert.throws(() => dns.lookup('www.google.com', 4),
              common.expectsError({code: 'ERR_INVALID_CALLBACK',
                                   type: TypeError}));

assert.doesNotThrow(() => dns.lookup('www.google.com', 6, noop));

assert.doesNotThrow(() => dns.lookup('www.google.com', {}, noop));

assert.doesNotThrow(() => dns.lookup('', {family: 4, hints: 0}, noop));

assert.doesNotThrow(() => {
  dns.lookup('', {
    family: 6,
    hints: dns.ADDRCONFIG
  }, noop);
});

assert.doesNotThrow(() => dns.lookup('', {hints: dns.V4MAPPED}, noop));

assert.doesNotThrow(() => {
  dns.lookup('', {
    hints: dns.ADDRCONFIG | dns.V4MAPPED
  }, noop);
});

assert.throws(() => dns.lookupService('0.0.0.0'),
              common.expectsError({code: 'ERR_INVALID_ARGS', type: Error}));

assert.throws(() => dns.lookupService('fasdfdsaf', 0, noop),
              common.expectsError({code: 'ERR_INVALID_IP', type: Error}));

assert.doesNotThrow(() => dns.lookupService('0.0.0.0', '0', noop));

assert.doesNotThrow(() => dns.lookupService('0.0.0.0', 0, noop));

assert.throws(() => dns.lookupService('0.0.0.0', null, noop),
              common.expectsError({code: 'ERR_INVALID_PORT',
                                   type: RangeError}));

assert.throws(
  () => dns.lookupService('0.0.0.0', undefined, noop),
  common.expectsError({code: 'ERR_INVALID_PORT', type: RangeError})
);

assert.throws(() => dns.lookupService('0.0.0.0', 65538, noop),
              common.expectsError({code: 'ERR_INVALID_PORT',
                                   type: RangeError}));

assert.throws(() => dns.lookupService('0.0.0.0', 'test', noop),
              common.expectsError({code: 'ERR_INVALID_PORT',
                                   type: RangeError}));

assert.throws(() => dns.lookupService('0.0.0.0', 80, null),
              common.expectsError({code: 'ERR_INVALID_CALLBACK',
                                   type: Error}));
