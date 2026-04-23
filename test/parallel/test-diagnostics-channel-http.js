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

dc.subscribe('http.client.request.start', common.mustCall(({ request }) => {
  assert.strictEqual(isOutgoingMessage(request), true);
}, 4));

dc.subscribe('http.client.request.bodyChunkSent', common.mustCall(({ request, chunk, encoding }) => {
  assert.strictEqual(isOutgoingMessage(request), true);
  assert.ok(typeof chunk === 'string' || chunk instanceof Uint8Array);
  assert.strictEqual(typeof encoding === 'string' || encoding == null, true);
}, 3));

dc.subscribe('http.client.request.bodySent', common.mustCall(({ request }) => {
  assert.strictEqual(isOutgoingMessage(request), true);
}, 2));

dc.subscribe('http.client.request.error', common.mustCall(({ request, error }) => {
  assert.strictEqual(isOutgoingMessage(request), true);
  assert.strictEqual(isError(error), true);
}));

dc.subscribe('http.client.response.bodyChunkReceived', common.mustCall(({
  request,
  response,
  chunk,
}) => {
  assert.strictEqual(isOutgoingMessage(request), true);
  assert.strictEqual(isIncomingMessage(response), true);
  assert.ok(chunk instanceof Uint8Array);
}, 3));

dc.subscribe('http.client.response.finish', common.mustCall(({
  request,
  response
}) => {
  assert.strictEqual(isOutgoingMessage(request), true);
  assert.strictEqual(isIncomingMessage(response), true);
}, 3));

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
}, 3));

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
}, 3));

dc.subscribe('http.server.response.created', common.mustCall(({
  request,
  response,
}) => {
  assert.strictEqual(isIncomingMessage(request), true);
  assert.strictEqual(isOutgoingMessage(response), true);
}, 3));

dc.subscribe('http.client.request.created', common.mustCall(({ request }) => {
  assert.strictEqual(isOutgoingMessage(request), true);
  assert.strictEqual(isHTTPServer(server), true);
}, 4));

const server = http.createServer(common.mustCall((req, res) => {
  const chunks = [];
  req.on('data', (chunk) => chunks.push(chunk));
  req.on('end', common.mustCall(() => {
    if (req.method === 'POST' && req.url === '/string-body') {
      assert.strictEqual(Buffer.concat(chunks).toString(), 'foobar');
    } else if (req.method === 'POST' && req.url === '/binary-body') {
      assert.deepStrictEqual(Buffer.concat(chunks), Buffer.from([0, 1, 2, 3]));
    } else {
      assert.strictEqual(req.method, 'GET');
      assert.strictEqual(req.url, '/');
      assert.strictEqual(Buffer.concat(chunks).byteLength, 0);
    }
    res.end('done');
  }));
}, 3));

server.listen(async () => {
  const { port } = server.address();
  const invalidRequest = http.get({
    host: addresses.INVALID_HOST,
  });
  await new Promise((resolve) => {
    invalidRequest.on('error', resolve);
  });
  await new Promise((resolve, reject) => {
    http.get(`http://localhost:${port}`, (res) => {
      res.setEncoding('utf8');
      res.resume();
      res.on('end', resolve);
    }).on('error', reject);
  });
  await new Promise((resolve, reject) => {
    const req = http.request(`http://localhost:${port}/string-body`, {
      method: 'POST',
    }, (res) => {
      res.resume();
      res.on('end', resolve);
    });
    req.on('error', reject);
    req.write('foo');
    req.end('bar');
  });
  await new Promise((resolve, reject) => {
    const req = http.request(`http://localhost:${port}/binary-body`, {
      method: 'POST',
    }, (res) => {
      res.resume();
      res.on('end', resolve);
    });
    req.on('error', reject);
    req.end(Buffer.from([0, 1, 2, 3]));
  });
  server.close();
});
