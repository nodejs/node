// Flags: --experimental-dtls --no-warnings

// Test: ipv6Only, and dual stack by default.
//
// Bind() forced UV_UDP_IPV6ONLY for every IPv6 address, so an endpoint on ::
// could not be reached over IPv4 at all and there was no way to ask for the
// dual-stack socket that node:dgram and node:quic give by default.

import { hasCrypto, skip } from '../common/index.mjs';
import * as fixtures from '../common/fixtures.mjs';
import assert from 'node:assert';

if (!hasCrypto) {
  skip('missing crypto');
}

if (!process.features.dtls) {
  skip('DTLS is not enabled');
}

const { connect, listen } = await import('node:dtls');

const cert = fixtures.readKey('agent1-cert.pem').toString();
const key = fixtures.readKey('agent1-key.pem').toString();

// An IPv6 wildcard endpoint accepts IPv4 peers, which is the default
// everywhere else in Node.js.
{
  const arrived = Promise.withResolvers();
  let serverSession;

  const server = listen((session) => {
    serverSession ??= session;
    arrived.resolve();
  }, { cert, key, host: '::', port: 0 });

  const client = connect('127.0.0.1', server.address.port, {
    rejectUnauthorized: false,
    handshakeTimeout: 5000,
  });

  await client.opened;
  await arrived.promise;

  // The peer arrives mapped, which is what a dual-stack socket reports and
  // what anything keyed on the address will see.
  assert.match(serverSession.remoteAddress.address, /^::ffff:127\.0\.0\.1$/);
  assert.strictEqual(serverSession.remoteAddress.family, 'IPv6');

  await client.close();
  await server.close();
}

// ipv6Only: true asks for the old behaviour back.
{
  const server = listen(() => {}, {
    cert, key, host: '::', port: 0, ipv6Only: true,
  });

  const client = connect('127.0.0.1', server.address.port, {
    rejectUnauthorized: false,
    handshakeTimeout: 1500,
  });

  await assert.rejects(client.opened, { code: 'ERR_INVALID_STATE' });
  client.destroy();
  await server.close();
}

// It is a boolean, checked like the rest.
{
  for (const value of ['yes', 1, 0, null, {}]) {
    assert.throws(() => listen(() => {}, {
      cert, key, host: '::', port: 0, ipv6Only: value,
    }), { code: 'ERR_INVALID_ARG_TYPE' });
  }
}
