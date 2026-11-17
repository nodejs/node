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

// Connect to an HTTP/2 server (raw socket), start writing a POST request,
// receive the first chunk, then return the live request and server socket.
async function connect() {
  const server = net.createServer();
  server.listen(0);
  await once(server, 'listening');
  const clientSession =
    http2.connect(`http://localhost:${server.address().port}`);
  const [serverSocket] = await once(server, 'connection');
  serverSocket.on('error', () => {});
  serverSocket.resume();
  serverSocket.write(new h2test.SettingsFrame().data);
  serverSocket.write(new h2test.SettingsFrame(true).data);
  await once(clientSession, 'connect');

  const request = clientSession.request({ ':method': 'POST' });
  request.on('error', common.mustNotCall());
  request.write('request remains open');
  serverSocket.write(Buffer.concat([
    new h2test.HeadersFrame(streamId, responseHeaders).data,
    new h2test.DataFrame(streamId, first).data,
  ]));
  await once(request, 'response');

  const close = () => {
    clientSession.destroy();
    serverSocket.destroy();
    server.close();
  };

  return { request, serverSocket, close };
}

function finishResponse(serverSocket, code) {
  serverSocket.write(Buffer.concat([
    new h2test.DataFrame(streamId, last).data,
    new h2test.HeadersFrame(streamId, responseTrailers, 0, true).data,
    new h2test.RstStreamFrame(streamId, code).data,
  ]));
}

(async () => {
  for (const code of [
    http2.constants.NGHTTP2_NO_ERROR,
    http2.constants.NGHTTP2_CANCEL,
  ]) {
    // Start the request
    const { request, serverSocket, close } = await connect();
    const [chunk] = await once(request, 'data');
    assert.deepStrictEqual(chunk, first);
    request.pause();

    // End with a last response chunk + trailers + RST_STREAM
    finishResponse(serverSocket, code);
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
    close();
  }
})().then(common.mustCall());
