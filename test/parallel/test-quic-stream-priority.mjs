// Flags: --experimental-quic --no-warnings

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import assert from 'node:assert';
import * as fixtures from '../common/fixtures.mjs';

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('node:quic');
const { createPrivateKey } = await import('node:crypto');

const key = createPrivateKey(fixtures.readKey('agent1-key.pem'));
const cert = fixtures.readKey('agent1-cert.pem');

// ============================================================================
// Test 1: Priority returns null for non-HTTP/3 sessions
{
  const done = Promise.withResolvers();

  const serverEndpoint = await listen(mustCall((serverSession) => {
    serverSession.onstream = mustCall((stream) => {
      // Non-H3 session should not support priority
      assert.strictEqual(stream.priority, null);
      // setPriority should be a no-op (not throw)
      stream.setPriority({ level: 'high', incremental: true });
      assert.strictEqual(stream.priority, null);
      serverSession.close();
      done.resolve();
    });
  }), {
    sni: { '*': { keys: [key], certs: [cert] } },
    alpn: ['quic-test'],
  });

  const clientSession = await connect(serverEndpoint.address, {
    alpn: 'quic-test',
  });
  await clientSession.opened;

  const stream = await clientSession.createBidirectionalStream();
  // Client side, non-H3 — priority should be null
  assert.strictEqual(stream.priority, null);

  await done.promise;
  clientSession.close();
}

// ============================================================================
// Test 2: Validation of createStream priority/incremental options
{
  const done = Promise.withResolvers();

  const serverEndpoint = await listen(mustCall((serverSession) => {
    serverSession.opened.then(() => {
      done.resolve();
      serverSession.close();
    }).then(mustCall());
  }), {
    sni: { '*': { keys: [key], certs: [cert] } },
  });

  const clientSession = await connect(serverEndpoint.address, {
    servername: 'localhost',
  });
  await clientSession.opened;

  // Invalid priority level
  await assert.rejects(
    clientSession.createBidirectionalStream({ priority: 'urgent' }),
    { code: 'ERR_INVALID_ARG_VALUE' },
  );
  await assert.rejects(
    clientSession.createBidirectionalStream({ priority: 42 }),
    { code: 'ERR_INVALID_ARG_VALUE' },
  );

  // Invalid incremental value
  await assert.rejects(
    clientSession.createBidirectionalStream({ incremental: 'yes' }),
    { code: 'ERR_INVALID_ARG_TYPE' },
  );
  await assert.rejects(
    clientSession.createBidirectionalStream({ incremental: 1 }),
    { code: 'ERR_INVALID_ARG_TYPE' },
  );

  await done.promise;
  clientSession.close();
}

// ============================================================================
// Test 3: Validation of setPriority options
{
  const done = Promise.withResolvers();

  const serverEndpoint = await listen(mustCall((serverSession) => {
    serverSession.onstream = mustCall((stream) => {
      // Valid setPriority calls should not throw
      stream.setPriority({ level: 'high' });
      stream.setPriority({ level: 'low', incremental: true });
      stream.setPriority({ level: 'default', incremental: false });

      // Invalid level
      assert.throws(
        () => stream.setPriority({ level: 'urgent' }),
        { code: 'ERR_INVALID_ARG_VALUE' },
      );

      // Invalid incremental
      assert.throws(
        () => stream.setPriority({ incremental: 'yes' }),
        { code: 'ERR_INVALID_ARG_TYPE' },
      );

      // Not an object
      assert.throws(
        () => stream.setPriority('high'),
        { code: 'ERR_INVALID_ARG_TYPE' },
      );

      serverSession.close();
      done.resolve();
    });
  }), {
    sni: { '*': { keys: [key], certs: [cert] } },
  });

  const clientSession = await connect(serverEndpoint.address, {
    servername: 'localhost',
  });
  await clientSession.opened;

  await clientSession.createBidirectionalStream();

  await done.promise;
  clientSession.close();
}
