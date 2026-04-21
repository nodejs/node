// Flags: --experimental-quic --no-warnings

// Test: SNI with multiple entries.
// Server has 3+ SNI entries. Different servername values should
// negotiate successfully using the correct identity.

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import assert from 'node:assert';
import * as fixtures from '../common/fixtures.mjs';

const { strictEqual } = assert;
const { readKey } = fixtures;

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('node:quic');
const { createPrivateKey } = await import('node:crypto');

const key1 = createPrivateKey(readKey('agent1-key.pem'));
const cert1 = readKey('agent1-cert.pem');
const key2 = createPrivateKey(readKey('agent2-key.pem'));
const cert2 = readKey('agent2-cert.pem');
const key3 = createPrivateKey(readKey('agent3-key.pem'));
const cert3 = readKey('agent3-cert.pem');

let sessionCount = 0;
const allDone = Promise.withResolvers();

const serverEndpoint = await listen(mustCall(async (serverSession) => {
  const info = await serverSession.opened;
  // Each client should negotiate with the correct servername.
  strictEqual(typeof info.servername, 'string');
  serverSession.close();
  await serverSession.closed;
  if (++sessionCount === 3) allDone.resolve();
}, 3), {
  sni: {
    'host1.example.com': { keys: [key1], certs: [cert1] },
    'host2.example.com': { keys: [key2], certs: [cert2] },
    '*': { keys: [key3], certs: [cert3] },
  },
  alpn: ['quic-test'],
});

// Client 1: connects with servername 'host1.example.com'.
{
  const cs = await connect(serverEndpoint.address, {
    servername: 'host1.example.com',
    alpn: 'quic-test',
  });
  const info = await cs.opened;
  strictEqual(info.servername, 'host1.example.com');
  await cs.closed;
}

// Client 2: connects with servername 'host2.example.com'.
{
  const cs = await connect(serverEndpoint.address, {
    servername: 'host2.example.com',
    alpn: 'quic-test',
  });
  const info = await cs.opened;
  strictEqual(info.servername, 'host2.example.com');
  await cs.closed;
}

// Client 3: connects with servername 'unknown.example.com' → wildcard.
{
  const cs = await connect(serverEndpoint.address, {
    servername: 'unknown.example.com',
    alpn: 'quic-test',
  });
  const info = await cs.opened;
  assert.strictEqual(info.servername, 'unknown.example.com');
  await cs.closed;
}

await allDone.promise;
await serverEndpoint.close();
