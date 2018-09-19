'use strict';

require('../common');
const common = require('../common');
const dns = require('dns');
const dnsPromises = dns.promises;
const { addresses } = require('../common/internet');
const assert = require('assert');

assert.rejects(
  dnsPromises.lookup(addresses.INVALID_HOST, {
    hints: 0,
    family: 0,
    all: false
  }),
  {
    code: 'ENOTFOUND',
    message: `getaddrinfo ENOTFOUND ${addresses.INVALID_HOST}`
  }
);

assert.rejects(
  dnsPromises.lookup(addresses.INVALID_HOST, {
    hints: 0,
    family: 0,
    all: true
  }),
  {
    code: 'ENOTFOUND',
    message: `getaddrinfo ENOTFOUND ${addresses.INVALID_HOST}`
  }
);

dns.lookup(addresses.INVALID_HOST, {
  hints: 0,
  family: 0,
  all: true
}, common.mustCall((error) => {
  assert.strictEqual(error.code, 'ENOTFOUND');
  assert.strictEqual(
    error.message,
    `getaddrinfo ENOTFOUND ${addresses.INVALID_HOST}`
  );
  assert.strictEqual(error.syscall, 'getaddrinfo');
  assert.strictEqual(error.hostname, addresses.INVALID_HOST);
}));
