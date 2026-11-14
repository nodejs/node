'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const http2 = require('http2');
const net = require('net');
const { once } = require('events');

// maxSessionMemory is documented to reject new streams while the limit is
// exceeded. Make sure that also holds for requests initiated by a client
// session, so that a peer that does not read cannot make the client queue
// frames without bound. As for response headers, request bodies that are
// queued waiting for flow control are not counted, so a large upload does not
// prevent unrelated requests on the same session.

async function queuedUploadDoesNotBlockRequests() {
  // The server never opens its flow-control window, so the upload stays queued
  // in the client session.
  const server = http2.createServer({ settings: { initialWindowSize: 0 } });
  server.on('stream', common.mustCall((stream) => {
    stream.on('error', () => {});
  }, 2));
  server.listen(0);
  await once(server, 'listening');

  const client = http2.connect(`http://localhost:${server.address().port}`, {
    maxSessionMemory: 1,
  });
  client.on('error', common.mustNotCall());
  await once(client, 'remoteSettings');

  const uploadArrived = once(server, 'stream');
  const upload = client.request({ ':method': 'POST', ':path': '/upload' });
  upload.on('error', () => {});
  upload.end(Buffer.alloc(4 * 1024 * 1024));
  await uploadArrived;

  const other = client.request({ ':path': '/other' });
  other.on('error', () => {});
  await once(server, 'stream');

  other.destroy();
  upload.destroy();
  client.destroy();
  server.close();
}

// A minimal HTTP/2 peer on a plain TCP socket, so that it can genuinely stop
// reading. It acknowledges SETTINGS and answers every request with
// `:status: 200` and END_STREAM, which closes the stream.
function respondingPeer(socket) {
  socket.write(Buffer.from([0, 0, 0, 4, 0, 0, 0, 0, 0]));  // SETTINGS
  let buffered = Buffer.alloc(0);
  let prefaceSeen = false;
  socket.on('data', (chunk) => {
    buffered = Buffer.concat([buffered, chunk]);
    if (!prefaceSeen) {
      if (buffered.length < 24) return;
      buffered = buffered.subarray(24);
      prefaceSeen = true;
    }
    while (buffered.length >= 9) {
      const length = buffered.readUIntBE(0, 3);
      if (buffered.length < 9 + length) return;
      const type = buffered[3];
      const flags = buffered[4];
      const streamId = buffered.readUInt32BE(5) & 0x7fffffff;
      buffered = buffered.subarray(9 + length);
      if (type === 4 && !(flags & 1)) {
        socket.write(Buffer.from([0, 0, 0, 4, 1, 0, 0, 0, 0]));  // SETTINGS ACK
      } else if (type === 1) {
        // HEADERS, END_STREAM | END_HEADERS, ':status: 200' (static index 8).
        const frame = Buffer.from([0, 0, 1, 1, 5, 0, 0, 0, 0, 0x88]);
        frame.writeUInt32BE(streamId, 5);
        socket.write(frame);
      }
    }
  });
}

async function stalledPeerRejectsRequestsAndSessionRecovers(stall) {
  // Requests are submitted in batches that yield to the event loop, so their
  // frames are written to the socket. While the peer answers them, streams
  // close and nothing is rejected; once it stops reading, no responses arrive,
  // so streams stay open and accumulate until maxSessionMemory rejects new
  // requests.
  let peerSocket;
  const server = net.createServer((socket) => {
    peerSocket = socket;
    respondingPeer(socket);
  });
  server.listen(0);
  await once(server, 'listening');

  const client = http2.connect(`http://localhost:${server.address().port}`, {
    maxSessionMemory: 1,
  });
  client.on('error', () => {});
  await once(client, 'remoteSettings');
  if (stall) peerSocket.pause();

  let rejection;
  for (let batch = 0; batch < 200 && rejection === undefined; batch++) {
    for (let i = 0; i < 100; i++) {
      const req = client.request({ ':path': '/' });
      req.on('error', (err) => {
        if (err.code === 'ERR_HTTP2_SESSION_MEMORY_EXCEEDED') rejection ??= err;
      });
      req.end();
    }
    await new Promise(setImmediate);
  }

  if (!stall) {
    assert.strictEqual(rejection, undefined);
    client.destroy();
    peerSocket.destroy();
    server.close();
    return;
  }

  assert.ok(rejection, 'expected a request to be rejected');
  assert.strictEqual(
    rejection.message,
    'The request was not sent because the session exceeded maxSessionMemory');
  assert.strictEqual(client.destroyed, false);
  assert.strictEqual(client.closed, false);

  // Once the peer reads again it answers the open streams, which close and
  // free session memory, so a request that was rejected can be retried on the
  // same session.
  peerSocket.resume();
  let accepted = false;
  for (let attempt = 0; attempt < 200 && !accepted; attempt++) {
    // An accepted request gets its stream ID synchronously.
    const req = client.request({ ':path': '/retry' });
    // An accepted request gets its stream ID synchronously.
    req.end();
    if (!req.pending) {
      accepted = true;
      break;
    }
    const outcome = await new Promise((resolve) => {
      req.once('ready', () => resolve('ready'));
      req.once('error', (err) => resolve(err.code));
    });
    accepted = outcome === 'ready';
    if (!accepted) {
      assert.strictEqual(outcome, 'ERR_HTTP2_SESSION_MEMORY_EXCEEDED');
      await new Promise((resolve) => setTimeout(resolve, 10));
    }
  }
  assert.ok(accepted, 'expected a retried request to be accepted');
  assert.strictEqual(client.destroyed, false);

  client.destroy();
  peerSocket.destroy();
  server.close();
}

(async () => {
  await queuedUploadDoesNotBlockRequests();
  await stalledPeerRejectsRequestsAndSessionRecovers(false);
  await stalledPeerRejectsRequestsAndSessionRecovers(true);
})().then(common.mustCall());
