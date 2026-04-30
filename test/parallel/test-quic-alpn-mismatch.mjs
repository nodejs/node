// Flags: --experimental-quic --no-warnings

// Test: ALPN mismatch causes connection failure.
// The server offers 'quic-test' but the client requests 'nonexistent'.
// The handshake should fail.

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import assert from 'node:assert';

const { rejects, strictEqual } = assert;

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('../common/quic.mjs');

const onerror = mustCall((err) => {
  strictEqual(err.code, 'ERR_QUIC_TRANSPORT_ERROR');
}, 2);
const transportParams = { maxIdleTimeout: 1 };

const serverEndpoint = await listen(mustCall(async (serverSession) => {
  await rejects(serverSession.opened, {
    code: 'ERR_QUIC_TRANSPORT_ERROR',
  });
  await rejects(serverSession.closed, {
    code: 'ERR_QUIC_TRANSPORT_ERROR',
  });
}), {
  transportParams,
  onerror,
});

// Client requests an ALPN the server doesn't offer.
const clientSession = await connect(serverEndpoint.address, {
  alpn: 'nonexistent-protocol',
  transportParams,
  onerror,
});

await rejects(clientSession.opened, {
  code: 'ERR_QUIC_TRANSPORT_ERROR',
});

// The handshake should fail — opened may reject or never resolve.
// The session should close with an error.
await rejects(clientSession.closed, {
  code: 'ERR_QUIC_TRANSPORT_ERROR',
});
