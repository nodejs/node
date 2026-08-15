// Flags: --experimental-quic --no-warnings

// Test: endpoint binding options.
// Binding to specific address.
// Binding to specific port.

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import assert from 'node:assert';
import * as fixtures from '../common/fixtures.mjs';

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect, QuicEndpoint } = await import('node:quic');
const { createPrivateKey } = await import('node:crypto');

const key = createPrivateKey(fixtures.readKey('agent1-key.pem'));
const cert = fixtures.readKey('agent1-cert.pem');

// Binding to a specific port.
{
  const endpoint = new QuicEndpoint({
    address: { address: '127.0.0.1', port: 0 },
  });

  const serverEndpoint = await listen(mustCall(async (serverSession) => {
    await serverSession.closed;
  }), {
    endpoint,
    sni: { '*': { keys: [key], certs: [cert] } },
    alpn: ['quic-test'],
    transportParams: { maxIdleTimeout: 1 },
  });

  // The address should reflect what we bound to.
  const addr = serverEndpoint.address;
  assert.strictEqual(addr.address, '127.0.0.1');
  assert.strictEqual(addr.family, 'ipv4');
  assert.strictEqual(typeof addr.port, 'number');
  assert.ok(addr.port > 0, 'port should be assigned');

  // Verify a client can connect to the bound address.
  const clientSession = await connect(serverEndpoint.address, {
    alpn: 'quic-test',
    verifyPeer: 'manual',
    transportParams: { maxIdleTimeout: 1 },
  });
  await clientSession.opened;
  await clientSession.close();

  await serverEndpoint.close();
}
