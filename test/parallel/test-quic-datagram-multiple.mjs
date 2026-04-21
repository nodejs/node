// Flags: --experimental-quic --experimental-stream-iter --no-warnings

// Test: multiple datagrams and datagrams alongside streams
// Client sends multiple datagrams.
// Datagrams sent alongside an active bidi stream.
// Datagrams are unreliable — we verify at least some arrive.
// The stream is opened first to establish bidirectional traffic,
// keeping the congestion window healthy for datagram sends.

import { hasQuic, skip, mustCall, mustCallAtLeast } from '../common/index.mjs';
import assert from 'node:assert';
import * as fixtures from '../common/fixtures.mjs';

const { ok } = assert;
const { readKey } = fixtures;

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('node:quic');
const { createPrivateKey } = await import('node:crypto');
const { bytes } = await import('stream/iter');

const key = createPrivateKey(readKey('agent1-key.pem'));
const cert = readKey('agent1-cert.pem');

const numDatagrams = 5;
let serverDatagramCount = 0;
const gotSomeDg = Promise.withResolvers();
const streamDone = Promise.withResolvers();

const serverEndpoint = await listen(mustCall(async (serverSession) => {
  await serverSession.opened;

  // Server receives stream data alongside datagrams.
  serverSession.onstream = mustCall(async (stream) => {
    stream.writer.endSync();
    await bytes(stream);
    await stream.closed;
    streamDone.resolve();
  });

  await Promise.all([gotSomeDg.promise, streamDone.promise]);
  await serverSession.close();
}), {
  sni: { '*': { keys: [key], certs: [cert] } },
  alpn: ['quic-test'],
  transportParams: { maxDatagramFrameSize: 1200 },
  ondatagram: mustCallAtLeast((data) => {
    ok(data instanceof Uint8Array);
    serverDatagramCount++;
    gotSomeDg.resolve();
  }),
});

const clientSession = await connect(serverEndpoint.address, {
  alpn: 'quic-test',
  transportParams: { maxDatagramFrameSize: 1200 },
});

await clientSession.opened;

// Open a stream FIRST to establish bidirectional traffic.
// This ensures ACKs flow back from the server, keeping the
// congestion window open for subsequent datagram sends.
const stream = await clientSession.createBidirectionalStream({
  body: new TextEncoder().encode('hello'),
});

// Send multiple datagrams alongside the active stream.
for (let i = 0; i < numDatagrams; i++) {
  await clientSession.sendDatagram(new Uint8Array([i]));
}

// Complete the stream.
for await (const _ of stream) { /* drain */ } // eslint-disable-line no-unused-vars
await stream.closed;

// At least some datagrams should have arrived.
ok(serverDatagramCount > 0, 'Server should have received at least one datagram');

await clientSession.closed;
await serverEndpoint.close();
