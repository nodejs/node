// Flags: --no-warnings
'use strict';

// Tests multiple aspects of QuicSocket multiple endpoint support

const common = require('../common');
const { once } = require('events');
if (!common.hasQuic)
  common.skip('missing quic');

const assert = require('assert');

const { key, cert, ca } = require('../common/quic');

const { createQuicSocket } = require('net');

async function Test1(options, address) {
  const server = createQuicSocket(options);
  server.on('close', common.mustCall());

  const endpoint = server.endpoints[0];

  assert.strictEqual(endpoint.bound, false);
  assert.deepStrictEqual({}, endpoint.address);

  await endpoint.bind();

  assert.strictEqual(endpoint.bound, true);
  assert.strictEqual(endpoint.destroyed, false);
  assert.strictEqual(typeof endpoint.address.port, 'number');
  assert.strictEqual(endpoint.address.address, address);

  await endpoint.close();

  assert.strictEqual(endpoint.destroyed, true);
}

async function Test2() {
  // Creates a server with multiple endpoints (one on udp4 and udp6)
  const server = createQuicSocket({ endpoint: { type: 'udp6' } });
  server.addEndpoint();
  assert.strictEqual(server.endpoints.length, 2);
  assert.strictEqual(server.endpoints[0].bound, false);
  assert.deepStrictEqual({}, server.endpoints[0].address);

  server.listen({ key, cert, ca, alpn: 'zzz' });

  // Attempting to add an endpoint after fails.
  assert.throws(() => server.addEndpoint(), {
    code: 'ERR_INVALID_STATE'
  });

  await once(server, 'ready');

  assert.strictEqual(server.endpoints.length, 2);

  {
    const endpoint = server.endpoints[0];
    assert.strictEqual(endpoint.bound, true);
    assert.strictEqual(endpoint.destroyed, false);
    assert.strictEqual(endpoint.address.family, 'IPv6');
    assert.strictEqual(typeof endpoint.address.port, 'number');
    assert.strictEqual(endpoint.address.address, '::');
  }

  {
    const endpoint = server.endpoints[1];
    assert.strictEqual(endpoint.bound, true);
    assert.strictEqual(endpoint.destroyed, false);
    assert.strictEqual(endpoint.address.family, 'IPv4');
    assert.strictEqual(typeof endpoint.address.port, 'number');
    assert.strictEqual(endpoint.address.address, '0.0.0.0');
  }

  server.close();
  for (const endpoint of server.endpoints)
    assert.strictEqual(endpoint.destroyed, true);
}

const tests = [
  Test1({}, '0.0.0.0'),
  Test1({ endpoint: { port: 0 } }, '0.0.0.0'),
  Test1({ endpoint: { address: '127.0.0.1', port: 0 } }, '127.0.0.1'),
  Test1({ endpoint: { address: 'localhost', port: 0 } }, '127.0.0.1')
];

if (common.hasIPv6) {
  tests.push(
    Test1({ endpoint: { type: 'udp6' } }, '::'),
    Test1({ endpoint: { type: 'udp6', address: 'localhost' } }, '::1'),
    Test2());
}

Promise.all(tests);
