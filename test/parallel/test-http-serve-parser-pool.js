'use strict';

const common = require('../common');
const assert = require('assert');
const http = require('http');
const net = require('net');

const serveServer = http.serve(common.mustCall(() => new Response('serve')));

serveServer.listen(0, common.mustCall(() => {
  const client = net.createConnection(serveServer.address().port, common.mustCall(() => {
    client.end('GET / HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n');
  }));
  client.resume();
  client.on('end', common.mustCall(() => {
    serveServer.close(common.mustCall(runCreateServer));
  }));
}));

function runCreateServer() {
  const server = http.createServer(common.mustCall((request, response) => {
    response.end('createServer');
  }));

  server.listen(0, common.mustCall(() => {
    const client = net.createConnection(server.address().port, common.mustCall(() => {
      client.end('GET / HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n');
    }));

    let data = '';
    client.setEncoding('utf8');
    client.on('data', (chunk) => data += chunk);
    client.on('end', common.mustCall(() => {
      assert.match(data, /createServer/);
      server.close();
    }));
  }));
}
