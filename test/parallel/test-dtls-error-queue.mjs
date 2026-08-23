// Flags: --experimental-dtls --no-warnings

// Test: DTLS does not leave entries behind in the OpenSSL error queue.
//
// The queue is per-thread and shared with every other OpenSSL consumer in the
// process. DTLS spends most of its time handling unauthenticated input, so
// failures are routine: rejected handshakes, and DTLSv1_listen() choking on
// garbage datagrams from the accept path. If those entries are not discarded
// they are picked up by whatever crypto operation runs next and reported as
// its error -- e.g. crypto.createPrivateKey() failing with
// "SSL routines::record too small".

import { hasCrypto, skip } from '../common/index.mjs';
import * as fixtures from '../common/fixtures.mjs';
import assert from 'node:assert';
import crypto from 'node:crypto';
import dgram from 'node:dgram';

if (!hasCrypto) {
  skip('missing crypto');
}

if (!process.features.dtls) {
  skip('DTLS is not enabled');
}

const { connect, listen } = await import('node:dtls');

const cert = fixtures.readKey('agent1-cert.pem');
const key = fixtures.readKey('agent1-key.pem');
const ca1 = fixtures.readKey('ca1-cert.pem');
const ca2 = fixtures.readKey('ca2-cert.pem');

// The server accepts each session (cookie exchange completes) and only the
// client rejects, so sessions do arrive here; nothing needs doing with them.
const endpoint = listen(() => {}, {
  cert, key, ca: [ca1], host: '127.0.0.1', port: 0,
});
const { port } = endpoint.address;

// Provoke failures from both directions.

// 1. Rejected handshakes: the server certificate does not chain to ca2.
for (let i = 0; i < 3; i++) {
  const client = connect('127.0.0.1', port, {
    servername: 'agent1',
    ca: [ca2],
  });
  await assert.rejects(client.opened, (err) => {
    assert.match(err.message, /certificate verify failed/);
    return true;
  });
}

// 2. Garbage datagrams: each one reaches DTLSv1_listen() and fails there.
// The leading 22 makes them look enough like a DTLS handshake record to get
// past a cheap length/type screen.
const socket = dgram.createSocket('udp4');
for (let i = 0; i < 32; i++) {
  socket.send(Buffer.from([22, 254, 253, 0, 0, i & 0xff]), port, '127.0.0.1');
}
// Let the datagrams reach the endpoint and be processed before closing;
// dgram.close() does not flush queued sends.
await new Promise((resolve) => setTimeout(resolve, 200));
await new Promise((resolve) => socket.close(resolve));

// An unrelated crypto failure must report its own cause. Before the error
// queue was drained this surfaced a stale "SSL routines::..." entry left by
// the DTLS activity above, with the real DECODER error demoted into
// opensslErrorStack.
assert.throws(
  () => crypto.createPrivateKey(
    '-----BEGIN PRIVATE KEY-----\nZm9v\n-----END PRIVATE KEY-----\n'),
  (err) => {
    assert.doesNotMatch(
      err.message, /SSL routines/,
      `crypto error contaminated by a stale DTLS entry: ${err.message}`);
    return true;
  },
);

await endpoint.close();
