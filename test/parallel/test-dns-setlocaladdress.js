'use strict';
require('../common');
const assert = require('assert');

const dns = require('dns');
const resolver = new dns.Resolver();
const promiseResolver = new dns.promises.Resolver();

// Verifies that setLocalAddress succeeds with IPv4 and IPv6 addresses
{
  resolver.setLocalAddress('127.0.0.1');
  resolver.setLocalAddress('::1');
  resolver.setLocalAddress('127.0.0.1', '::1');
  promiseResolver.setLocalAddress('127.0.0.1', '::1');
}

// Verify that setLocalAddress throws if called with an invalid address
{
  assert.throws(() => {
    resolver.setLocalAddress('127.0.0.1', '127.0.0.1');
  }, Error);
  assert.throws(() => {
    resolver.setLocalAddress('::1', '::1');
  }, Error);
  assert.throws(() => {
    resolver.setLocalAddress('bad');
  }, Error);
  assert.throws(() => {
    resolver.setLocalAddress(123);
  }, { code: 'ERR_INVALID_ARG_TYPE' });
  assert.throws(() => {
    resolver.setLocalAddress('127.0.0.1', 42);
  }, { code: 'ERR_INVALID_ARG_TYPE' });
  assert.throws(() => {
    resolver.setLocalAddress();
  }, Error);
  assert.throws(() => {
    promiseResolver.setLocalAddress();
  }, Error);
}

// Verify that setLocalAddress rejects addresses containing null bytes instead
// of silently truncating them at the first null byte
{
  for (const r of [resolver, promiseResolver]) {
    assert.throws(() => {
      r.setLocalAddress('127.0.0.1\u0000junk');
    }, {
      code: 'ERR_INVALID_ARG_VALUE',
      name: 'TypeError',
      message: /^The argument 'ipv4' must be a string without null bytes\. Received /,
    });
    assert.throws(() => {
      r.setLocalAddress('127.0.0.1', '::1\u0000junk');
    }, {
      code: 'ERR_INVALID_ARG_VALUE',
      name: 'TypeError',
      message: /^The argument 'ipv6' must be a string without null bytes\. Received /,
    });
  }
}
