// Flags: --experimental-quic --experimental-stream-iter --no-warnings

// Test: HTTP/3 high volume transfer through constrained flow control
// windows, in both directions.
//
// HTTP/3 credits inbound flow control differently from raw QUIC. The bytes
// consumed by nghttp3 frame parsing are credited as soon as they are read,
// but DATA frame *payload* is deliberately excluded from that and is instead
// credited later, when the application actually consumes it. That makes the
// HTTP/3 receive path a genuinely separate code path from raw QUIC rather
// than a thin wrapper over it, so it needs its own volume coverage: a
// mis-credit here (either double counting the payload or never crediting it)
// would not be caught by the raw QUIC tests.
//
// A large request body and a large response body are both pushed through
// windows far smaller than either, so the transfer only completes if the
// payload is credited exactly once as it is consumed.

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import assert from 'node:assert';
import * as fixtures from '../common/fixtures.mjs';

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('node:quic');
const { createPrivateKey } = await import('node:crypto');
const { makePayload, hashBytes } = await import('../common/quic.mjs');
const { bytes } = await import('stream/iter');

const key = createPrivateKey(fixtures.readKey('agent1-key.pem'));
const cert = fixtures.readKey('agent1-cert.pem');

const kTotal = 2 * 1024 * 1024;         // per direction
const kStreamWindow = 16 * 1024;
const kConnWindow = 32 * 1024;

assert.ok(kTotal / kStreamWindow >= 100,
          'payload must be far larger than the window to be meaningful');

// Distinct payloads per direction so a direction mixup is detectable.
const requestBody = makePayload(kTotal, 31);
const responseBody = makePayload(kTotal, 41);
const requestHash = hashBytes(requestBody);
const responseHash = hashBytes(responseBody);
assert.notStrictEqual(requestHash, responseHash);

const serverDone = Promise.withResolvers();

const serverEndpoint = await listen(mustCall(async (serverSession) => {
  serverSession.onstream = mustCall(async (stream) => {
    // Read the large request body. This is the path where DATA payload
    // credit is deferred until consumption.
    const body = await bytes(stream);
    assert.strictEqual(body.byteLength, kTotal);
    // Must be the request body, intact and in order.
    assert.strictEqual(hashBytes(body), requestHash);

    await stream.closed;
    serverSession.close();
    serverDone.resolve();
  });
}), {
  sni: { '*': { keys: [key], certs: [cert] } },
  transportParams: {
    initialMaxStreamDataBidiRemote: kStreamWindow,
    initialMaxData: kConnWindow,
  },
  maxStreamWindow: kStreamWindow,
  maxWindow: kConnWindow,
  onheaders: mustCall(function(headers) {
    assert.strictEqual(headers[':method'], 'POST');
    this.sendHeaders({ ':status': '200', 'content-type': 'application/octet-stream' });
    // Send a large response body concurrently with reading the request
    // body, so both directions are flow-control bound at the same time.
    this.setBody(responseBody);
  }),
});

const clientSession = await connect(serverEndpoint.address, {
  servername: 'localhost',
  verifyPeer: 'manual',
  transportParams: {
    initialMaxStreamDataBidiLocal: kStreamWindow,
    initialMaxData: kConnWindow,
  },
  maxStreamWindow: kStreamWindow,
  maxWindow: kConnWindow,
});

const info = await clientSession.opened;
assert.strictEqual(info.protocol, 'h3');

const headersReceived = Promise.withResolvers();

const stream = await clientSession.createBidirectionalStream({
  headers: {
    ':method': 'POST',
    ':path': '/upload',
    ':scheme': 'https',
    ':authority': 'localhost',
  },
  body: requestBody,
  onheaders: mustCall(function(headers) {
    assert.strictEqual(headers[':status'], 200);
    headersReceived.resolve();
  }),
});

await headersReceived.promise;

const received = await bytes(stream);
assert.strictEqual(received.byteLength, kTotal);
// Must be the response body, intact and in order.
assert.strictEqual(hashBytes(received), responseHash);

await Promise.all([stream.closed, serverDone.promise]);
await clientSession.close();
await serverEndpoint.close();
