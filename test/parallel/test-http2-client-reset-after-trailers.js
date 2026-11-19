'use strict';

// A server may send the final DATA, trailers with END_STREAM and
// RST_STREAM(NO_ERROR) together while the request is still open (RFC 9113
// section 8.1), as gRPC servers do. A client that has paused partway through
// the response must still be able to read the rest, and likewise for
// RST_STREAM(CANCEL), which should only affect the request.
// This closely reproduces https://github.com/nodejs/node/issues/66693
// which requires some raw frame writes to match the reset behaviour.

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const http2 = require('http2');
const net = require('net');
const { once } = require('events');
const { setImmediate: setImmediateAsync } = require('timers/promises');
const h2test = require('../common/http2');

const first = Buffer.from('first chunk');
const last = Buffer.from('last chunk');
// HPACK: indexed :status 200.
const responseHeaders = Buffer.from([0x88]);
// HPACK: literal without indexing, grpc-status: 0.
const responseTrailers = Buffer.concat([
  Buffer.from([0x00, 11]), Buffer.from('grpc-status'),
  Buffer.from([1]), Buffer.from('0'),
]);
const streamId = 1;
const startRequest = 'start of request';
const continueRequest = 'more of request';

async function startServer(code) {
  const server = net.createServer(common.mustCall((socket) => {
    // Client request -> response frames flow for the server:
    const replies = [
      [startRequest, [
        new h2test.HeadersFrame(streamId, responseHeaders),
        new h2test.DataFrame(streamId, first),
      ]],
      [continueRequest, [
        new h2test.DataFrame(streamId, last),
        new h2test.HeadersFrame(streamId, responseTrailers, 0, true),
        new h2test.RstStreamFrame(streamId, code),
      ]],
    ];

    let received = Buffer.alloc(0);
    socket.on('error', () => {});
    socket.on('data', (chunk) => {
      received = Buffer.concat([received, chunk]);
      while (replies.length > 0 && received.includes(replies[0][0])) {
        const [, frames] = replies.shift();
        socket.write(Buffer.concat(frames.map((frame) => frame.data)));
      }
    });

    // Write settings to start the session:
    socket.write(new h2test.SettingsFrame().data);
    socket.write(new h2test.SettingsFrame(true).data);
  }));
  server.listen(0);
  await once(server, 'listening');
  return server;
}

(async () => {
  for (const code of [
    http2.constants.NGHTTP2_NO_ERROR,
    http2.constants.NGHTTP2_CANCEL,
  ]) {
    const server = await startServer(code);
    const session = http2.connect(`http://localhost:${server.address().port}`);
    const request = session.request({ ':method': 'POST' });
    request.on('error', common.mustNotCall());
    request.on('response', common.mustCall());

    // Read the first response chunk, then stop reading.
    request.write(startRequest);
    const [chunk] = await once(request, 'data');
    assert.deepStrictEqual(chunk, first);
    request.pause();

    // Trigger the last response chunk + trailers + RST_STREAM.
    request.write(continueRequest);
    const [trailers] = await once(request, 'trailers');
    assert.strictEqual(trailers['grpc-status'], '0');

    while (!request.closed)
      await setImmediateAsync();
    assert.strictEqual(request.rstCode, code);
    assert.strictEqual(request.destroyed, false);

    // Check that we can still read the response after the RST:
    const chunks = [];
    request.on('data', (chunk) => chunks.push(chunk));
    request.on('end', common.mustCall());
    request.resume();
    await once(request, 'close');
    assert.deepStrictEqual(Buffer.concat(chunks), last);

    session.destroy();
    server.close();
  }
})().then(common.mustCall());
