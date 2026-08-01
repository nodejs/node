// Flags: --experimental-dtls --no-warnings

// Test: MTU option validation and that a small (but valid) MTU still completes
// a handshake and data exchange (exercising DTLS record fragmentation).

import { hasCrypto, skip, mustCall } from '../common/index.mjs';
import assert from 'node:assert';
import * as fixtures from '../common/fixtures.mjs';

const { strictEqual, throws } = assert;
const { readKey } = fixtures;

if (!hasCrypto) {
  skip('missing crypto');
}

if (!process.features.dtls) {
  skip('DTLS is not enabled');
}

const { listen, connect, DTLSEndpoint } = await import('node:dtls');

const cert = readKey('agent1-cert.pem').toString();
const key = readKey('agent1-key.pem').toString();
const ca = readKey('ca1-cert.pem').toString();

// MTU must be within [256, 65535].
throws(() => new DTLSEndpoint({ mtu: 255 }), { code: 'ERR_OUT_OF_RANGE' });
throws(() => new DTLSEndpoint({ mtu: 65536 }), { code: 'ERR_OUT_OF_RANGE' });

// A valid boundary MTU is accepted.
{
  const endpoint = new DTLSEndpoint({ mtu: 256 });
  await endpoint.close();
}

// A small valid MTU forces the handshake flights (e.g. the certificate) to be
// fragmented across records, and the exchange still succeeds.
{
  const received = Promise.withResolvers();

  const server = listen(mustCall((session) => {
    session.onmessage = mustCall((data) => {
      strictEqual(data.toString(), 'hello');
      session.send('world');
    });
  }), {
    cert, key, port: 0, host: '127.0.0.1', mtu: 256,
  });

  const client = connect('127.0.0.1', server.address.port, {
    ca: [ca],
    rejectUnauthorized: false,
    mtu: 256,
  });

  client.onmessage = mustCall((data) => {
    strictEqual(data.toString(), 'world');
    received.resolve();
  });

  await client.opened;
  client.send('hello');
  await received.promise;

  await client.close();
  await server.close();
}
