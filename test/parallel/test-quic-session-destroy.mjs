// Flags: --experimental-quic --no-warnings

// Test: session.destroy() forceful close.
// destroy() without error resolves the closed promise.
// destroy(error) rejects the closed promise with that error.
// destroy() works without a prior close() call.
// Note: destroy() is forceful and does not send CONNECTION_CLOSE.
// The server session remains alive until idle timeout unless we also
// destroy the server session explicitly. We use a short idle timeout
// to keep the tests fast, and destroy both sides in each section.

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import assert from 'node:assert';

const { strictEqual, rejects } = assert;

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('../common/quic.mjs');

// Use a short idle timeout so the server session cleans up quickly
// after the client destroys without CONNECTION_CLOSE.
const transportParams = { maxIdleTimeout: 1 };

// -------------------------------------------------------------------
// destroy() without error resolves the closed promise.
// -------------------------------------------------------------------
{
  const serverDone = Promise.withResolvers();

  const serverEndpoint = await listen(mustCall(async (serverSession) => {
    await serverSession.closed;
    serverDone.resolve();
  }), { transportParams });

  const clientSession = await connect(serverEndpoint.address, {
    transportParams,
  });
  await clientSession.opened;

  clientSession.destroy();
  strictEqual(clientSession.destroyed, true);

  // Closed should resolve (no error).
  await clientSession.closed;

  await serverDone.promise;
  await serverEndpoint.close();
}

// -------------------------------------------------------------------
// destroy(error) rejects closed with that error.
// -------------------------------------------------------------------
{
  const serverDone = Promise.withResolvers();

  const serverEndpoint = await listen(mustCall(async (serverSession) => {
    await serverSession.closed;
    serverDone.resolve();
  }), { transportParams });

  const clientSession = await connect(serverEndpoint.address, {
    transportParams,
  });
  await clientSession.opened;

  const testError = new Error('intentional destroy error');
  clientSession.destroy(testError);
  strictEqual(clientSession.destroyed, true);

  // Closed should reject with the same error.
  await rejects(clientSession.closed, testError);

  await serverDone.promise;
  await serverEndpoint.close();
}

// -------------------------------------------------------------------
// destroy() works without prior close().
// -------------------------------------------------------------------
{
  const serverDone = Promise.withResolvers();

  const serverEndpoint = await listen(mustCall(async (serverSession) => {
    await serverSession.closed;
    serverDone.resolve();
  }), { transportParams });

  const clientSession = await connect(serverEndpoint.address, {
    transportParams,
  });
  await clientSession.opened;

  // Destroy directly without calling close() first.
  clientSession.destroy();
  strictEqual(clientSession.destroyed, true);
  await clientSession.closed;

  await serverDone.promise;
  await serverEndpoint.close();
}
