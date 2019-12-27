// Flags: --no-warnings
'use strict';

// Tests multiple aspects of QuicSocket multiple endpoint support

const common = require('../common');
const { once } = require('events');
if (!common.hasQuic)
  common.skip('missing quic');

const assert = require('assert');
const fixtures = require('../common/fixtures');
const key = fixtures.readKey('agent1-key.pem', 'binary');
const cert = fixtures.readKey('agent1-cert.pem', 'binary');
const ca = fixtures.readKey('ca1-cert.pem', 'binary');

const { createSocket } = require('quic');

async function Test1(options, address) {
  const server = createSocket(options);
  assert.strictEqual(server.endpoints.length, 1);
  assert.strictEqual(server.endpoints[0].bound, false);
  assert.deepStrictEqual({}, server.endpoints[0].address);

  server.listen({ key, cert, ca, alpn: 'zzz' })

  await once(server, 'ready');

  assert.strictEqual(server.endpoints.length, 1);

  const endpoint = server.endpoints[0];
  assert.strictEqual(endpoint.bound, true);
  assert.strictEqual(endpoint.closing, false);
  assert.strictEqual(endpoint.destroyed, false);
  assert.strictEqual(typeof endpoint.address.port, 'number');
  assert.strictEqual(endpoint.address.address, address);

  server.close();
  assert.strictEqual(endpoint.destroyed, true);
}

async function Test2(options) {
  // Creates a server with multiple endpoints (one on udp4 and udp6)
  const server = createSocket({ endpoint: { type: 'udp6' }});
  server.addEndpoint();
  assert.strictEqual(server.endpoints.length, 2);
  assert.strictEqual(server.endpoints[0].bound, false);
  assert.deepStrictEqual({}, server.endpoints[0].address);

  server.listen({ key, cert, ca, alpn: 'zzz' })

  await once(server, 'ready');

  assert.strictEqual(server.endpoints.length, 2);

  {
    const endpoint = server.endpoints[0];
    assert.strictEqual(endpoint.bound, true);
    assert.strictEqual(endpoint.closing, false);
    assert.strictEqual(endpoint.destroyed, false);
    assert.strictEqual(endpoint.address.family, 'IPv6');
    assert.strictEqual(typeof endpoint.address.port, 'number');
    assert.strictEqual(endpoint.address.address, '::');
  }

  {
    const endpoint = server.endpoints[1];
    assert.strictEqual(endpoint.bound, true);
    assert.strictEqual(endpoint.closing, false);
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
