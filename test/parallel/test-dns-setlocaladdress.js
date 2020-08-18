'use strict';
require('../common');
const assert = require('assert');

const dns = require('dns');
const resolver = new dns.Resolver();

// Verifies that setLocalAddress succeeds with IPv4 and IPv6 addresses
{
  resolver.setLocalAddress('127.0.0.1');
  resolver.setLocalAddress('::1');
}

// Verifiy that setLocalAddress throws if called with an invalid address
{
  assert.throws(() => {
    resolver.setLocalAddress('bad');
  }, Error);
  assert.throws(() => {
    resolver.setLocalAddress(123);
  }, Error);
  assert.throws(() => {
    resolver.setLocalAddress();
  }, Error);
}
