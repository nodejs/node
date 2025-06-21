'use strict';

const common = require('../common');

if (!common.hasCrypto)
  common.skip('missing crypto');

const https = require('https');
const http = require('http');
const net = require('net');
const assert = require('assert');
const tls = require('tls');
const { readKey } = require('../common/fixtures');

function request(socket) {
  socket.write('GET / HTTP/1.1\r\nHost: localhost\r\nConnection: keep-alive\r\n\r\n\r\n');
}

// https options
const httpsOptions = {
  key: readKey('agent1-key.pem'),
  cert: readKey('agent1-cert.pem')
};

const server = https.createServer(httpsOptions, common.mustCall((req, res) => {
  res.end('ok');
}));

server.on('dropRequest', common.mustCall((request, socket) => {
  assert.strictEqual(request instanceof http.IncomingMessage, true);
  assert.strictEqual(socket instanceof net.Socket, true);
  server.close();
}));

server.listen(0, common.mustCall(() => {
  const socket = tls.connect(
    server.address().port,
    {
      rejectUnauthorized: false
    },
    common.mustCall(() => {
      request(socket);
      request(socket);
      socket.on('error', common.mustNotCall());
      socket.on('data', common.mustCallAtLeast());
      socket.on('close', common.mustCall());
    })
  );
}));

server.maxRequestsPerSocket = 1;
