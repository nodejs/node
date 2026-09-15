'use strict';
const common = require('../common');
const { addresses } = require('../common/internet');
const assert = require('assert');
const http = require('http');
const net = require('net');
const dc = require('diagnostics_channel');

const isHTTPServer = (server) => server instanceof http.Server;
const isIncomingMessage = (object) => object instanceof http.IncomingMessage;
const isOutgoingMessage = (object) => object instanceof http.OutgoingMessage;
const isNetSocket = (socket) => socket instanceof net.Socket;
const isError = (error) => error instanceof Error;
let postBodyChunkSent = 0;
let postBodySent = false;
let clientResponseBodyChunksReceived = 0;

const verifyPostBody = common.mustCall(() => {
  assert.strictEqual(postBodySent, true);
  assert.strictEqual(clientResponseBodyChunksReceived, 2);
});

dc.subscribe('http.client.request.start', common.mustCall(({ request }) => {
  assert.strictEqual(isOutgoingMessage(request), true);
}, 3));

dc.subscribe('http.client.request.error', common.mustCall(({ request, error }) => {
  assert.strictEqual(isOutgoingMessage(request), true);
  assert.strictEqual(isError(error), true);
}));

dc.subscribe('http.client.response.finish', common.mustCall(({
  request,
  response
}) => {
  assert.strictEqual(isOutgoingMessage(request), true);
  assert.strictEqual(isIncomingMessage(response), true);
}, 2));

dc.subscribe('http.client.request.bodyChunkSent', common.mustCall(({
  request,
  chunk,
  encoding,
}) => {
  assert.strictEqual(isOutgoingMessage(request), true);
  assert.ok(typeof chunk === 'string' || chunk instanceof Uint8Array);
  assert.ok(
    typeof encoding === 'string' ||
    encoding === null ||
    encoding === undefined,
  );
  postBodyChunkSent++;
}, 2));

dc.subscribe('http.client.request.bodySent', common.mustCall(({ request }) => {
  assert.strictEqual(isOutgoingMessage(request), true);
  assert.strictEqual(postBodyChunkSent, 2);
  postBodySent = true;
}));

dc.subscribe('http.client.response.bodyChunkReceived', common.mustCall(({
  request,
  response,
  chunk,
}) => {
  assert.strictEqual(isOutgoingMessage(request), true);
  assert.strictEqual(isIncomingMessage(response), true);
  assert.ok(chunk instanceof Uint8Array);
  clientResponseBodyChunksReceived++;
}, 2));

dc.subscribe('http.server.request.start', common.mustCall(({
  request,
  response,
  socket,
  server,
}) => {
  assert.strictEqual(isIncomingMessage(request), true);
  assert.strictEqual(isOutgoingMessage(response), true);
  assert.strictEqual(isNetSocket(socket), true);
  assert.strictEqual(isHTTPServer(server), true);
}, 2));

dc.subscribe('http.server.response.finish', common.mustCall(({
  request,
  response,
  socket,
  server,
}) => {
  assert.strictEqual(isIncomingMessage(request), true);
  assert.strictEqual(isOutgoingMessage(response), true);
  assert.strictEqual(isNetSocket(socket), true);
  assert.strictEqual(isHTTPServer(server), true);
}, 2));

dc.subscribe('http.server.response.created', common.mustCall(({
  request,
  response,
}) => {
  assert.strictEqual(isIncomingMessage(request), true);
  assert.strictEqual(isOutgoingMessage(response), true);
}, 2));

dc.subscribe('http.client.request.created', common.mustCall(({ request }) => {
  assert.strictEqual(isOutgoingMessage(request), true);
  assert.strictEqual(isHTTPServer(server), true);
}, 3));

const server = http.createServer(common.mustCall((req, res) => {
  req.resume();
  req.on('end', () => {
    res.end('done');
  });
}, 2));

server.listen(common.mustCall(async () => {
  const { port } = server.address();
  const invalidRequest = http.get({
    host: addresses.INVALID_HOST,
  });
  await new Promise((resolve) => {
    invalidRequest.on('error', resolve);
  });
  http.get(`http://localhost:${port}`, common.mustCall((res) => {
    res.resume();
    res.on('end', common.mustCall(() => {
      const post = http.request({
        hostname: 'localhost',
        port,
        method: 'POST',
      }, common.mustCall((postRes) => {
        postRes.resume();
        postRes.on('end', common.mustCall(() => {
          verifyPostBody();
          server.close();
        }));
      }));
      post.write('foo');
      post.end(Buffer.from('bar'));
    }));
  }));
}));
