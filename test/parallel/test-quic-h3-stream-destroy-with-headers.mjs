// Flags: --experimental-quic --no-warnings

// Test: Stream with pending headers destroyed before send.
// Creating an H3 stream with headers and immediately destroying it
// should clean up without crashing or leaking.

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import assert from 'node:assert';
import * as fixtures from '../common/fixtures.mjs';

const { strictEqual } = assert;
const { readKey } = fixtures;

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('node:quic');
const { createPrivateKey } = await import('node:crypto');

const key = createPrivateKey(readKey('agent1-key.pem'));
const cert = readKey('agent1-cert.pem');

const serverDone = Promise.withResolvers();

const serverEndpoint = await listen(mustCall(async (ss) => {
  // The server may or may not see the stream depending on timing.
  // Either way, it should not crash.
  await ss.closed;
  serverDone.resolve();
}), {
  sni: { '*': { keys: [key], certs: [cert] } },
});

const clientSession = await connect(serverEndpoint.address, {
  servername: 'localhost',
});
await clientSession.opened;

// Create a stream with headers, then immediately destroy it.
const stream = await clientSession.createBidirectionalStream({
  headers: {
    ':method': 'GET',
    ':path': '/destroyed',
    ':scheme': 'https',
    ':authority': 'localhost',
  },
});

// Destroy the stream before headers can be sent/processed.
stream.destroy();

// Verify the stream is destroyed without crash.
strictEqual(stream.destroyed, true);

// Close everything cleanly.
clientSession.close();
await serverDone.promise;
