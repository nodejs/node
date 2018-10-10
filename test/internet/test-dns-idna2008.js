'use strict';

// Verify that non-ASCII hostnames are handled correctly as IDNA 2008.
//
// * Tests will fail with NXDOMAIN when UTF-8 leaks through to a getaddrinfo()
//   that doesn't support IDNA at all.
//
// * "straße.de" will resolve to the wrong address when the resolver supports
//   only IDNA 2003 (e.g., glibc until 2.28) because it encodes it wrong.

const { mustCall } = require('../common');
const assert = require('assert');
const dns = require('dns');

const [host, expectedAddress] = ['straße.de', '81.169.145.78'];

dns.lookup(host, mustCall((err, address) => {
  assert.ifError(err);
  assert.strictEqual(address, expectedAddress);
}));

dns.promises.lookup(host).then(mustCall(({ address }) => {
  assert.strictEqual(address, expectedAddress);
}));

dns.resolve4(host, mustCall((err, addresses) => {
  assert.ifError(err);
  assert.deepStrictEqual(addresses, [expectedAddress]);
}));

new dns.promises.Resolver().resolve4(host).then(mustCall((addresses) => {
  assert.deepStrictEqual(addresses, [expectedAddress]);
}));
