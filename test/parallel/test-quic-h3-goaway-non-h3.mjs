// Flags: --experimental-quic --experimental-stream-iter --no-warnings

// Test: Non-H3 session close does not fire ongoaway.
// GOAWAY is an HTTP/3 concept. When a non-H3 session closes, the
// ongoaway callback must not fire.

import { hasQuic, skip, mustCall, mustNotCall } from '../common/index.mjs';
import assert from 'node:assert';
import { setImmediate } from 'node:timers/promises';
import * as fixtures from '../common/fixtures.mjs';

const { strictEqual } = assert;
const { readKey } = fixtures;

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('node:quic');
const { createPrivateKey } = await import('node:crypto');
const { bytes } = await import('stream/iter');

const key = createPrivateKey(readKey('agent1-key.pem'));
const cert = readKey('agent1-cert.pem');
const encoder = new TextEncoder();
const decoder = new TextDecoder();

const serverDone = Promise.withResolvers();

const serverEndpoint = await listen(mustCall(async (ss) => {
  ss.onstream = mustCall(async (stream) => {
    // Read client data, send response, close stream.
    const data = await bytes(stream);
    strictEqual(decoder.decode(data), 'ping');
    stream.writer.writeSync('pong');
    stream.writer.endSync();
    await stream.closed;
    ss.close();
    serverDone.resolve();
  });
}), {
  sni: { '*': { keys: [key], certs: [cert] } },
  alpn: 'quic-test',
});

const clientSession = await connect(serverEndpoint.address, {
  servername: 'localhost',
  alpn: 'quic-test',
  // Ongoaway must NOT fire for non-H3 sessions.
  ongoaway: mustNotCall(),
});
await clientSession.opened;

const stream = await clientSession.createBidirectionalStream({
  body: encoder.encode('ping'),
});

const response = await bytes(stream);
strictEqual(decoder.decode(response), 'pong');
await Promise.all([stream.closed, serverDone.promise]);

// Wait a tick for any deferred callbacks to fire.
await setImmediate();

clientSession.close();
