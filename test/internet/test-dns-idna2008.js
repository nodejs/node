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
const { addresses } = require('../common/internet');

const fixture = {
  hostname: 'straße.de',
  expectedAddress: '81.169.145.78',
  dnsServer: addresses.DNS4_SERVER
};

// Explicitly use well behaved DNS servers that are known to be able to resolve
// the query (which is a.k.a xn--strae-oqa.de).
dns.setServers([fixture.dnsServer]);

dns.lookup(fixture.hostname, mustCall((err, address) => {
  if (err && err.errno === 'ESERVFAIL') {
    assert.ok(err.message.includes('queryA ESERVFAIL straße.de'));
    return;
  }
  assert.ifError(err);
  assert.strictEqual(address, fixture.expectedAddress);
}));

dns.promises.lookup(fixture.hostname).then(({ address }) => {
  assert.strictEqual(address, fixture.expectedAddress);
}, (err) => {
  if (err && err.errno === 'ESERVFAIL') {
    assert.ok(err.message.includes('queryA ESERVFAIL straße.de'));
  } else {
    throw err;
  }
}).finally(mustCall());

dns.resolve4(fixture.hostname, mustCall((err, addresses) => {
  if (err && err.errno === 'ESERVFAIL') {
    assert.ok(err.message.includes('queryA ESERVFAIL straße.de'));
    return;
  }
  assert.ifError(err);
  assert.deepStrictEqual(addresses, [fixture.expectedAddress]);
}));

const p = new dns.promises.Resolver().resolve4(fixture.hostname);
p.then((addresses) => {
  assert.deepStrictEqual(addresses, [fixture.expectedAddress]);
}, (err) => {
  if (err && err.errno === 'ESERVFAIL') {
    assert.ok(err.message.includes('queryA ESERVFAIL straße.de'));
  } else {
    throw err;
  }
}).finally(mustCall());
