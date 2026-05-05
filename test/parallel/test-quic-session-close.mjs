// Flags: --experimental-quic --no-warnings

import { mustCall } from '../common/index.mjs';
import assert from 'node:assert';
import { checkQuic, createQuicPair, defaultCerts } from '../common/quic/helpers.mjs';

checkQuic();

const { listen, connect } = await import('node:quic');

// Test 1: session.close() returns a promise that resolves.
{
  const { clientSession, serverSession, endpoint } = await createQuicPair();

  const closePromise = clientSession.close();
  assert.ok(closePromise instanceof Promise,
            'close() should return a promise');

  await closePromise;

  // Closed should also resolve after close() completes.
  await clientSession.closed;

  serverSession.close();
  await serverSession.closed;
  await endpoint.close();
}

// Test 2: close() is idempotent -- calling it multiple times does not throw.
{
  const { clientSession, serverSession, endpoint } = await createQuicPair();

  clientSession.close();
  clientSession.close();  // Second call should not throw.
  await clientSession.closed;

  serverSession.close();
  await serverSession.closed;
  await endpoint.close();
}

// Test 3: Endpoint survives session close and can accept new connections.
{
  const { keys, certs } = await defaultCerts();
  let sessionCount = 0;

  const endpoint = await listen(mustCall((session) => {
    sessionCount++;
    session.opened.then(mustCall(() => {
      session.close();
    }));
  }, 2), { keys, certs });

  // First connection.
  const client1 = await connect(endpoint.address);
  await client1.opened;
  client1.close();
  await client1.closed;

  // Second connection on the same endpoint.
  const client2 = await connect(endpoint.address);
  await client2.opened;
  client2.close();
  await client2.closed;

  assert.strictEqual(sessionCount, 2);

  await endpoint.close();
}

// Test 4: session.closed resolves after close() on both client and server side.
{
  const { clientSession, serverSession, endpoint } = await createQuicPair();

  const clientClosed = Promise.withResolvers();
  const serverClosed = Promise.withResolvers();

  clientSession.closed.then(mustCall(() => {
    clientClosed.resolve();
  }));

  serverSession.closed.then(mustCall(() => {
    serverClosed.resolve();
  }));

  clientSession.close();
  serverSession.close();

  await Promise.all([clientClosed.promise, serverClosed.promise]);
  await endpoint.close();
}

// Test 5: session.destroyed is true after close completes.
{
  const { clientSession, serverSession, endpoint } = await createQuicPair();

  assert.strictEqual(clientSession.destroyed, false);

  clientSession.close();
  await clientSession.closed;

  assert.strictEqual(clientSession.destroyed, true);

  serverSession.close();
  await serverSession.closed;
  await endpoint.close();
}
