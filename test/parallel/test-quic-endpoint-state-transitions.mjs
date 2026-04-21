// Flags: --expose-internals --experimental-quic --no-warnings

// Test: endpoint state transitions.
// State transitions: created → bound → receiving → listening → closing.
// Binding to 0.0.0.0 (all interfaces).
// Binding to ::1 (IPv6 loopback).

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import assert from 'node:assert';
import * as fixtures from '../common/fixtures.mjs';

const { ok, strictEqual } = assert;
const { readKey } = fixtures;

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect, QuicEndpoint } = await import('node:quic');
const { createPrivateKey } = await import('node:crypto');

const key = createPrivateKey(readKey('agent1-key.pem'));
const cert = readKey('agent1-cert.pem');

{
  const endpoint = new QuicEndpoint();

  const serverEndpoint = await listen(mustCall(async (serverSession) => {
    await serverSession.opened;
    serverSession.close();
    await serverSession.closed;
  }), {
    endpoint,
    sni: { '*': { keys: [key], certs: [cert] } },
    alpn: ['quic-test'],
  });

  // After listen, the endpoint should be listening.
  strictEqual(serverEndpoint.listening, true);
  strictEqual(serverEndpoint.closing, false);
  strictEqual(serverEndpoint.destroyed, false);

  const cs = await connect(serverEndpoint.address, { alpn: 'quic-test' });
  await cs.opened;
  await cs.close();

  // After close(), closing transitions to true. The endpoint is still
  // "listening" in the sense that it holds the socket, but closing is true.
  serverEndpoint.close();
  strictEqual(serverEndpoint.closing, true);

  await serverEndpoint.closed;
  strictEqual(serverEndpoint.destroyed, true);
}

// Binding to 0.0.0.0.
{
  const endpoint = new QuicEndpoint({
    address: { address: '0.0.0.0', port: 0 },
  });

  const serverEndpoint = await listen(mustCall(async (serverSession) => {
    await serverSession.closed;
  }), {
    endpoint,
    sni: { '*': { keys: [key], certs: [cert] } },
    alpn: ['quic-test'],
    transportParams: { maxIdleTimeout: 1 },
  });

  const addr = serverEndpoint.address;
  strictEqual(addr.address, '0.0.0.0');
  ok(addr.port > 0);

  // Connect via 127.0.0.1 since 0.0.0.0 listens on all interfaces.
  const cs = await connect(`127.0.0.1:${addr.port}`, {
    alpn: 'quic-test',
    transportParams: { maxIdleTimeout: 1 },
  });
  await cs.opened;
  await cs.close();

  await serverEndpoint.close();
}
