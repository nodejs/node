// Flags: --experimental-quic --experimental-stream-iter --no-warnings

// Test: completing an HTTP/3 request returns exactly one unit of stream
// credit.
//
// initialMaxStreamsBidi = 1 lets the client hold one request stream open at
// a time. Every stream that finishes on the wire returns credit for exactly
// one more, so a batch of requests issued at once stays serialised for the
// life of the connection: the server must never see two live at the same
// time, and must not stall by losing credit unexpectedly.

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import assert from 'node:assert';
import * as fixtures from '../common/fixtures.mjs';

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('node:quic');
const { createPrivateKey } = await import('node:crypto');
const { bytes } = await import('stream/iter');

const key = createPrivateKey(fixtures.readKey('agent1-key.pem'));
const cert = fixtures.readKey('agent1-cert.pem');

const kRequests = 6;

let liveServerStreams = 0;
let peakLiveServerStreams = 0;

const serverEndpoint = await listen(mustCall((serverSession) => {
  serverSession.onstream = mustCall((stream) => {
    liveServerStreams++;
    peakLiveServerStreams = Math.max(peakLiveServerStreams, liveServerStreams);
    stream.closed.then(mustCall(() => { liveServerStreams--; }));
  }, kRequests);
}), {
  sni: { '*': { keys: [key], certs: [cert] } },
  // Only one client-initiated bidi stream may be open at a time.
  transportParams: { initialMaxStreamsBidi: 1 },
  onheaders: mustCall(function() {
    this.sendHeaders({ ':status': '200' });
    const w = this.writer;
    w.writeSync('ok');
    w.endSync();
  }, kRequests),
});

const clientSession = await connect(serverEndpoint.address, {
  servername: 'localhost',
  verifyPeer: 'manual',
});

const info = await clientSession.opened;
assert.strictEqual(info.protocol, 'h3');

// Issue all requests up front. The 1st will open, the others will be left
// pending and fire as the max-stream credit is returned.
const streams = [];
for (let i = 0; i < kRequests; i++) {
  streams.push(await clientSession.createBidirectionalStream({
    headers: {
      ':method': 'GET',
      ':path': `/${i}`,
      ':scheme': 'https',
      ':authority': 'localhost',
    },
  }));
}
assert.strictEqual(streams[0].pending, false);
assert.ok(streams.slice(1).every((s) => s.pending),
          'only one stream may open while the limit is 1');

const unexpectedStreamCount = () =>
  `server saw ${peakLiveServerStreams} streams open at once with a limit of 1`;

for (const stream of streams) {
  assert.ok(peakLiveServerStreams <= 1, unexpectedStreamCount());
  await bytes(stream);
  await stream.closed;
}

assert.strictEqual(peakLiveServerStreams, 1, unexpectedStreamCount());

await clientSession.close();
await serverEndpoint.close();
