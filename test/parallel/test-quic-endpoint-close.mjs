// Flags: --experimental-quic --no-warnings

import { mustCall } from '../common/index.mjs';
import assert from 'node:assert';
import { checkQuic, createQuicPair, defaultCerts } from '../common/quic/helpers.mjs';

checkQuic();

const { listen, connect } = await import('node:quic');

// Test 1: endpoint.close() with no active sessions resolves cleanly.
{
  const { keys, certs } = await defaultCerts();

  const endpoint = await listen(mustCall(0), { keys, certs });

  assert.strictEqual(endpoint.destroyed, false);
  assert.strictEqual(endpoint.closing, false);

  const closePromise = endpoint.close();
  assert.ok(closePromise instanceof Promise,
            'close() should return a promise');

  assert.strictEqual(endpoint.closing, true);

  await closePromise;
  assert.strictEqual(endpoint.destroyed, true);
}

// Test 2: endpoint.close() is idempotent.
{
  const { keys, certs } = await defaultCerts();

  const endpoint = await listen(mustCall(0), { keys, certs });

  endpoint.close();
  endpoint.close();  // Second call should not throw.

  await endpoint.closed;
  assert.strictEqual(endpoint.destroyed, true);
}

// Test 3: endpoint.destroy() forcefully tears down sessions.
{
  const { clientSession, serverSession, endpoint } = await createQuicPair();

  assert.strictEqual(endpoint.destroyed, false);
  assert.strictEqual(clientSession.destroyed, false);
  assert.strictEqual(serverSession.destroyed, false);

  // destroy() should trigger session destruction.
  endpoint.destroy();

  await endpoint.closed;
  assert.strictEqual(endpoint.destroyed, true);

  // Sessions should also be destroyed after endpoint.destroy().
  assert.strictEqual(serverSession.destroyed, true);

  // Client session may not be immediately destroyed since it's on its own
  // endpoint, but it should eventually close due to peer disconnect.
  // Wait with a reasonable timeout.
  await clientSession.closed.catch(() => {});
}

// Test 4: endpoint.destroy(error) rejects the closed promise with that error.
{
  const { keys, certs } = await defaultCerts();
  const endpoint = await listen(mustCall(0), { keys, certs });

  const testError = new Error('endpoint destroy error');

  await assert.rejects(async () => {
    endpoint.destroy(testError);
    await endpoint.closed;
  }, (err) => {
    assert.strictEqual(err, testError);
    return true;
  });
}

// Test 5: endpoint.close() with active sessions waits for sessions to end.
{
  const { clientSession, serverSession, endpoint } = await createQuicPair();

  // Initiate graceful close on endpoint. This should wait for sessions.
  const endpointClosed = endpoint.close();

  // The endpoint should be closing but not yet destroyed since sessions
  // are still active.
  assert.strictEqual(endpoint.closing, true);

  // Now close the sessions.
  clientSession.close();
  serverSession.close();

  await Promise.allSettled([clientSession.closed, serverSession.closed]);

  // The endpoint close should now complete.
  await endpointClosed;
  assert.strictEqual(endpoint.destroyed, true);
}

// Test 6: endpoint.closed reflects the same promise as close() return value.
{
  const { keys, certs } = await defaultCerts();
  const endpoint = await listen(mustCall(0), { keys, certs });

  const closeReturn = endpoint.close();
  const closedProp = endpoint.closed;

  assert.strictEqual(closeReturn, closedProp);

  await closeReturn;
}

// Test 7: listen() callback is not invoked for connections arriving
// after endpoint.close() has been called.
{
  const { keys, certs } = await defaultCerts();
  let unexpectedSession = false;

  const endpoint = await listen(() => {
    unexpectedSession = true;
  }, { keys, certs });

  const { address } = endpoint;

  // Close the endpoint before any client connects.
  await endpoint.close();

  assert.strictEqual(endpoint.destroyed, true);

  // Attempt a connection to the now-closed endpoint. The server
  // callback must not fire.
  const client = await connect(address);
  await client.opened.catch(() => {});
  client.destroy();
  await client.closed.catch(() => {});

  assert.strictEqual(unexpectedSession, false);
}
