'use strict';
const common = require('../common');
const assert = require('assert');
const http = require('http');
const net = require('net');
const dc = require('diagnostics_channel');

const onClientRequestStart = dc.channel('http.client.request.start');
const onClientResponseFinish = dc.channel('http.client.response.finish');
const onServerRequestStart = dc.channel('http.server.request.start');
const onServerResponseFinish = dc.channel('http.server.response.finish');

const isHTTPServer = (server) => server instanceof http.Server;
const isIncomingMessage = (object) => object instanceof http.IncomingMessage;
const isOutgoingMessage = (object) => object instanceof http.OutgoingMessage;
const isNetSocket = (socket) => socket instanceof net.Socket;

onClientRequestStart.subscribe(common.mustCall(({ request }) => {
  assert.strictEqual(isOutgoingMessage(request), true);
}));

onClientResponseFinish.subscribe(common.mustCall(({ request, response }) => {
  assert.strictEqual(isOutgoingMessage(request), true);
  assert.strictEqual(isIncomingMessage(response), true);
}));

onServerRequestStart.subscribe(common.mustCall(({
  request,
  response,
  socket,
  server,
}) => {
  assert.strictEqual(isIncomingMessage(request), true);
  assert.strictEqual(isOutgoingMessage(response), true);
  assert.strictEqual(isNetSocket(socket), true);
  assert.strictEqual(isHTTPServer(server), true);
}));

onServerResponseFinish.subscribe(common.mustCall(({
  request,
  response,
  socket,
  server,
}) => {
  assert.strictEqual(isIncomingMessage(request), true);
  assert.strictEqual(isOutgoingMessage(response), true);
  assert.strictEqual(isNetSocket(socket), true);
  assert.strictEqual(isHTTPServer(server), true);
}));

const server = http.createServer(common.mustCall((req, res) => {
  res.end('done');
}));

server.listen(() => {
  const { port } = server.address();
  http.get(`http://localhost:${port}`, (res) => {
    res.resume();
    res.on('end', () => {
      server.close();
    });
  });
});
