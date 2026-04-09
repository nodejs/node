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

const serverOpened = Promise.withResolvers();

const serverEndpoint = await listen(mustCall((serverSession) => {
  serverSession.opened.then(mustCall(() => {
    serverOpened.resolve();
    serverSession.close();
  }));
}), {
  sni: { '*': { keys: [key], certs: [cert] } },
  alpn: ['quic-test'],
});

const clientSession = await connect(serverEndpoint.address, {
  alpn: 'quic-test',
});
await clientSession.opened;
await serverOpened.promise;

// Test 1: Priority returns null for non-HTTP/3 sessions
{
  const stream = await clientSession.createBidirectionalStream();
  // Catch the closed rejection when the session closes with open streams
  stream.closed.catch(() => {});
  assert.strictEqual(stream.priority, null);

  // setPriority should be a no-op (not throw)
  stream.setPriority({ level: 'high', incremental: true });
  assert.strictEqual(stream.priority, null);
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

// Test 3: setPriority is a no-op on non-H3 sessions (does not throw
//         even with invalid arguments, because it returns early)
{
  const stream = await clientSession.createBidirectionalStream();
  stream.closed.catch(() => {});
  stream.setPriority({ level: 'high' });
  stream.setPriority({ level: 'low', incremental: true });
  stream.setPriority({ level: 'default', incremental: false });
  stream.setPriority({ level: 'urgent' });
  stream.setPriority('high');
}

clientSession.close();
