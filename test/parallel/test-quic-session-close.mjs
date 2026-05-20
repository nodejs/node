// Flags: --experimental-quic --no-warnings

// Test: session.close() lifecycle.
// session.close() with no open streams resolves the closed promise.
// Server-initiated close delivers CONNECTION_CLOSE to the client.
// Client-initiated close — server sees the session end.

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import assert from 'node:assert';

const { strictEqual } = assert;

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('../common/quic.mjs');

// -------------------------------------------------------------------
// session.close() with no open streams resolves closed.
// -------------------------------------------------------------------
{
  const serverEndpoint = await listen(mustCall(async (serverSession) => {
    // Server just waits for the client to close.
    await serverSession.closed;
  }));

  const clientSession = await connect(serverEndpoint.address);
  await clientSession.opened;

  // No streams opened. close() should resolve.
  await clientSession.close();
  strictEqual(clientSession.destroyed, true);
  await serverEndpoint.close();
}

// -------------------------------------------------------------------
// Server-initiated close — client sees session end.
// -------------------------------------------------------------------
{
  const serverEndpoint = await listen(mustCall(async (serverSession) => {
    await serverSession.opened;
    // Server initiates close.
    serverSession.close();
    await serverSession.closed;
  }));

  const clientSession = await connect(serverEndpoint.address);
  await clientSession.opened;

  // Client's closed promise should resolve when the server closes.
  await clientSession.closed;
  strictEqual(clientSession.destroyed, true);
  await serverEndpoint.close();
}

// -------------------------------------------------------------------
// Client-initiated close — server sees session end.
// -------------------------------------------------------------------
{
  const serverDone = Promise.withResolvers();

  const serverEndpoint = await listen(mustCall(async (serverSession) => {
    // The server's closed promise should resolve when the client closes.
    await serverSession.closed;
    strictEqual(serverSession.destroyed, true);
    serverDone.resolve();
  }));

  const clientSession = await connect(serverEndpoint.address);
  await clientSession.opened;

  // Client initiates close.
  await clientSession.close();
  await serverDone.promise;
  await serverEndpoint.close();
}
