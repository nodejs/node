// Flags: --experimental-dtls --no-warnings

// Test: DTLS over IPv6.
//
// listen() already accepted an IPv6 address, but connect() hardcoded the
// local bind to the IPv4 wildcard regardless of the peer, so the socket was
// AF_INET and could not send to an AF_INET6 destination. The bind address now
// follows the family of the remote literal.

import { hasCrypto, hasIPv6, skip } from '../common/index.mjs';
import * as fixtures from '../common/fixtures.mjs';
import assert from 'node:assert';

if (!hasCrypto) {
  skip('missing crypto');
}

if (!process.features.dtls) {
  skip('DTLS is not enabled');
}

if (!hasIPv6) {
  skip('no IPv6 support');
}

const { connect, listen } = await import('node:dtls');

const cert = fixtures.readKey('agent1-cert.pem');
const key = fixtures.readKey('agent1-key.pem');
const ca = fixtures.readKey('ca1-cert.pem');

// A full handshake and round trip over IPv6 loopback.
{
  const echoed = Promise.withResolvers();

  const endpoint = listen((session) => {
    session.onmessage = (data) => session.send(data);
  }, { cert, key, host: '::1', port: 0 });

  assert.strictEqual(endpoint.address.family, 'IPv6');

  const client = connect('::1', endpoint.address.port, {
    servername: 'agent1', ca: [ca],
  });

  await client.opened;
  client.onmessage = (data) => echoed.resolve(data.toString());
  client.send('over v6');

  assert.strictEqual(await echoed.promise, 'over v6');

  await client.close();
  await endpoint.close();
}

// The local socket picks the peer's family without being told. Binding the
// IPv4 wildcard here is what used to break the send.
{
  const endpoint = listen(() => {}, { cert, key, host: '::1', port: 0 });

  const client = connect('::1', endpoint.address.port, {
    servername: 'agent1', ca: [ca],
  });
  await client.opened;

  await client.close();
  await endpoint.close();
}

// An explicit bindHost is still honoured.
{
  const endpoint = listen(() => {}, { cert, key, host: '::1', port: 0 });

  const client = connect('::1', endpoint.address.port, {
    servername: 'agent1', ca: [ca], bindHost: '::',
  });
  await client.opened;

  await client.close();
  await endpoint.close();
}

// IPv4 is unaffected: the default must not become IPv6 for v4 peers.
{
  const endpoint = listen(() => {}, {
    cert, key, host: '127.0.0.1', port: 0,
  });

  assert.strictEqual(endpoint.address.family, 'IPv4');

  const client = connect('127.0.0.1', endpoint.address.port, {
    servername: 'agent1', ca: [ca],
  });
  await client.opened;

  await client.close();
  await endpoint.close();
}
