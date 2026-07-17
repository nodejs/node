// Flags: --experimental-quic --no-warnings

// Test: session.destroy(error) rejects both opened and closed promises
// .
// When destroyed before the handshake completes, both opened and closed
// reject. When destroyed after, opened stays resolved and closed rejects.

import { hasQuic, skip } from '../common/index.mjs';
import assert from 'node:assert';

const { rejects } = assert;

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('../common/quic.mjs');

const transportParams = { maxIdleTimeout: 1 };

// The server may see 1 or 2 sessions — the first client destroys before
// the handshake completes, so the server session may or may not be created.
const serverEndpoint = await listen(async (serverSession) => {
  await serverSession.closed;
}, { transportParams });

// First client: destroy BEFORE the handshake completes.
{
  const testError = new Error('early destroy');
  const clientSession = await connect(serverEndpoint.address, {
    transportParams,
  });

  // Destroy immediately — the handshake hasn't completed yet.
  clientSession.destroy(testError);

  // Both opened and closed should reject with the same error.
  await rejects(clientSession.opened, testError);
  await rejects(clientSession.closed, testError);
}

// Second client: destroy AFTER the handshake completes.
{
  const testError = new Error('late destroy');
  const clientSession = await connect(serverEndpoint.address, {
    transportParams,
  });
  await clientSession.opened;

  clientSession.destroy(testError);

  // Opened already resolved — stays resolved.
  await clientSession.opened;

  // Closed rejects with the error.
  await rejects(clientSession.closed, testError);
}

await serverEndpoint.close();
