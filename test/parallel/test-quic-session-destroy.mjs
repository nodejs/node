// Flags: --experimental-quic --no-warnings

import { mustCall } from '../common/index.mjs';
import assert from 'node:assert';
import { checkQuic, createQuicPair } from '../common/quic/helpers.mjs';

checkQuic();

// Test 1: destroy() immediately marks session as destroyed.
{
  const { clientSession, serverSession, endpoint } = await createQuicPair();

  assert.strictEqual(clientSession.destroyed, false);

  clientSession.destroy();

  assert.strictEqual(clientSession.destroyed, true);

  // Closed should still resolve when destroy is called without error.
  await clientSession.closed;

  serverSession.destroy();
  await serverSession.closed;
  await endpoint.close();
}

// Test 2: destroy() with no error resolves the closed promise.
{
  const { clientSession, serverSession, endpoint } = await createQuicPair();

  const closedResolved = Promise.withResolvers();

  clientSession.closed.then(mustCall(() => {
    closedResolved.resolve('resolved');
  })).catch(() => {
    closedResolved.resolve('rejected');
  });

  clientSession.destroy();

  const result = await closedResolved.promise;
  assert.strictEqual(result, 'resolved');

  serverSession.destroy();
  await serverSession.closed;
  await endpoint.close();
}

// Test 3: destroy(error) rejects the closed promise with that error.
{
  const { clientSession, serverSession, endpoint } = await createQuicPair();

  const testError = new Error('test destroy error');

  await assert.rejects(async () => {
    clientSession.destroy(testError);
    await clientSession.closed;
  }, (err) => {
    assert.strictEqual(err, testError);
    return true;
  });

  serverSession.destroy();
  await serverSession.closed;
  await endpoint.close();
}

// Test 4: destroy() is idempotent -- calling it after already destroyed is a no-op.
{
  const { clientSession, serverSession, endpoint } = await createQuicPair();

  clientSession.destroy();
  assert.strictEqual(clientSession.destroyed, true);

  // Second destroy should not throw.
  clientSession.destroy();
  assert.strictEqual(clientSession.destroyed, true);

  await clientSession.closed;
  serverSession.destroy();
  await serverSession.closed;
  await endpoint.close();
}

// Test 5: destroy() on server session.
{
  const { clientSession, serverSession, endpoint } = await createQuicPair();

  serverSession.destroy();

  assert.strictEqual(serverSession.destroyed, true);

  await serverSession.closed;

  // Client session may also close due to peer disconnect. Clean up.
  if (!clientSession.destroyed) {
    clientSession.destroy();
  }
  await clientSession.closed;
  await endpoint.close();
}
