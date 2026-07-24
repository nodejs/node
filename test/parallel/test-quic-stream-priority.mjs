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

const serverEndpoint = await listen(mustCall(async (serverSession) => {
  await serverSession.opened;
  await serverSession.close();
}), {
  sni: { '*': { keys: [key], certs: [cert] } },
  alpn: ['quic-test'],
});

const clientSession = await connect(serverEndpoint.address, {
  alpn: 'quic-test',
  verifyPeer: 'manual',
});
await clientSession.opened;

// Collect stream.closed promises so we can await them all at the end.
// We must not await them inline because the server's CONNECTION_CLOSE
// arrives asynchronously and would put the session into a closing state,
// preventing subsequent createBidirectionalStream calls.
const streamClosedPromises = [];

// Test 1: Priority getter returns null for non-HTTP/3 sessions.
//         setPriority throws because the session doesn't support priority.
{
  const stream = await clientSession.createBidirectionalStream();
  streamClosedPromises.push(stream.closed);
  assert.strictEqual(stream.priority, null);

  assert.throws(
    () => stream.setPriority({ level: 'high', incremental: true }),
    { code: 'ERR_INVALID_STATE' },
  );
}

// Test 2: Validation of createStream priority/incremental options
{
  await assert.rejects(
    clientSession.createBidirectionalStream({ priority: 'urgent' }),
    { code: 'ERR_INVALID_ARG_VALUE' },
  );
  await assert.rejects(
    clientSession.createBidirectionalStream({ priority: 42 }),
    { code: 'ERR_INVALID_ARG_VALUE' },
  );
  await assert.rejects(
    clientSession.createBidirectionalStream({ incremental: 'yes' }),
    { code: 'ERR_INVALID_ARG_TYPE' },
  );
  await assert.rejects(
    clientSession.createBidirectionalStream({ incremental: 1 }),
    { code: 'ERR_INVALID_ARG_TYPE' },
  );
}

// Test 3: setPriority throws on non-H3 sessions regardless of arguments
{
  const stream = await clientSession.createBidirectionalStream();
  streamClosedPromises.push(stream.closed);

  assert.throws(
    () => stream.setPriority({ level: 'high' }),
    { code: 'ERR_INVALID_STATE' },
  );
  assert.throws(
    () => stream.setPriority({ level: 'low', incremental: true }),
    { code: 'ERR_INVALID_STATE' },
  );
  assert.throws(
    () => stream.setPriority(),
    { code: 'ERR_INVALID_STATE' },
  );
}

// Wait for all streams to close (they close when the session closes
// in response to the server's CONNECTION_CLOSE).
await Promise.all(streamClosedPromises);
await clientSession.close();
await serverEndpoint.close();
