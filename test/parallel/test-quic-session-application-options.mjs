// Flags: --experimental-quic --experimental-stream-iter --no-warnings

// Test: session.applicationOptions
// Verifies that the settings an HTTP/3 session was given are reported back by
// applicationOptions as a null-prototype object on both peers, and become
// null once the session is gone.

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import assert from 'node:assert';

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('../common/quic.mjs');
const { Http3Session } = await import('node:quic');

const customSettings = {
  maxHeaderPairs: 50n,
  maxHeaderLength: 8192n,
  maxFieldSectionSize: 16384n,
  qpackMaxDTableCapacity: 2048n,
  qpackEncoderMaxDTableCapacity: 2048n,
  qpackBlockedStreams: 50n,
  enableConnectProtocol: false,
  enableDatagrams: false,
};

// Both peers advertise the same settings, so the values stay put when the
// peer's SETTINGS frame is applied on top of them.
function check(settings, side) {
  assert.ok(settings != null, `${side} settings should be available`);
  assert.strictEqual(typeof settings, 'object');
  assert.strictEqual(Object.getPrototypeOf(settings), null);
  assert.strictEqual(settings.maxHeaderPairs, customSettings.maxHeaderPairs);
  assert.strictEqual(settings.maxHeaderLength, customSettings.maxHeaderLength);
  assert.strictEqual(settings.maxFieldSectionSize,
                     customSettings.maxFieldSectionSize);
  assert.strictEqual(settings.qpackMaxDtableCapacity,
                     customSettings.qpackMaxDTableCapacity);
  assert.strictEqual(settings.qpackEncoderMaxDtableCapacity,
                     customSettings.qpackEncoderMaxDTableCapacity);
  assert.strictEqual(settings.qpackBlockedStreams,
                     customSettings.qpackBlockedStreams);
  assert.strictEqual(settings.enableConnectProtocol,
                     customSettings.enableConnectProtocol);
  assert.strictEqual(settings.enableDatagrams, customSettings.enableDatagrams);
}

const serverDone = Promise.withResolvers();

const serverEndpoint = await listen(mustCall((quicSession) => {
  const server = new Http3Session(quicSession, { settings: customSettings });
  quicSession.onstream = mustCall(async (stream) => {
    check(quicSession.applicationOptions, 'server');
    await stream.closed;
    server.close();
    serverDone.resolve();
  });
}), {
  alpn: ['h3'],
  onheaders: mustCall(function() {
    this.sendHeaders({ ':status': '200' });
    this.writer.endSync();
  }),
});

const client = new Http3Session(
  await connect(serverEndpoint.address, { alpn: 'h3' }),
  { settings: customSettings });

// The settings are in effect from the attach onwards: before the handshake
// completes, and before any SETTINGS frame from the peer can have arrived.
check(client.quicSession.applicationOptions, 'client');
await client.opened;
check(client.quicSession.applicationOptions, 'client');

// Exchange a request to let the server side run its assertions.
const stream = await client.createBidirectionalStream({
  headers: {
    ':method': 'GET',
    ':path': '/',
    ':scheme': 'https',
    ':authority': 'localhost',
  },
  onheaders: mustCall(),
});

// eslint-disable-next-line no-unused-vars
for await (const _ of stream) { /* drain */ }
await Promise.all([stream.closed, serverDone.promise]);

await client.close();
assert.strictEqual(client.quicSession.applicationOptions, null);

await serverEndpoint.close();
