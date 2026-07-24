// Flags: --experimental-quic --no-warnings

// Test: toJSON() and inspect() on stats objects.
// Verifies that stats objects from endpoints and sessions
// support toJSON() and util.inspect().

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import assert from 'node:assert';
import { inspect } from 'node:util';

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('../common/quic.mjs');

const serverDone = Promise.withResolvers();

const serverEndpoint = await listen(mustCall((serverSession) => {
  // Session stats toJSON and inspect.
  const sessionStatsJson = serverSession.stats.toJSON();
  assert.ok(sessionStatsJson);
  assert.strictEqual(typeof sessionStatsJson.createdAt, 'string');
  assert.strictEqual(typeof sessionStatsJson.bytesSent, 'string');

  const sessionStatsInspect = inspect(serverSession.stats);
  assert.ok(sessionStatsInspect.includes('QuicSessionStats'));

  serverSession.onstream = mustCall(async (stream) => {
    for await (const _ of stream) { /* drain */ } // eslint-disable-line no-unused-vars
    stream.writer.endSync();
    await stream.closed;
    serverSession.close();
    serverDone.resolve();
  });
}));

// Endpoint stats toJSON and inspect.
const endpointStatsJson = serverEndpoint.stats.toJSON();
assert.ok(endpointStatsJson);
assert.strictEqual(typeof endpointStatsJson.createdAt, 'string');

const endpointStatsInspect = inspect(serverEndpoint.stats);
assert.ok(endpointStatsInspect.includes('QuicEndpointStats'));

const clientSession = await connect(serverEndpoint.address);
await clientSession.opened;

// Client session stats.
const clientStatsJson = clientSession.stats.toJSON();
assert.ok(clientStatsJson);
assert.strictEqual(typeof clientStatsJson.createdAt, 'string');

const clientStatsInspect = inspect(clientSession.stats);
assert.ok(clientStatsInspect.includes('QuicSessionStats'));

const stream = await clientSession.createBidirectionalStream({
  body: new TextEncoder().encode('test'),
});
for await (const _ of stream) { /* drain */ } // eslint-disable-line no-unused-vars

await Promise.all([stream.closed, serverDone.promise]);

await clientSession.closed;
await serverEndpoint.close();
