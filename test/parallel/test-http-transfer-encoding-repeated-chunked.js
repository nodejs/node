'use strict';

const common = require('../common');
const assert = require('assert');

const http = require('http');
const net = require('net');

const msg = [
  'POST / HTTP/1.1',
  'Host: 127.0.0.1',
  'Transfer-Encoding: chunkedchunked',
  '',
  '1',
  'A',
  '0',
  '',
].join('\r\n');

const server = http.createServer(common.mustCall((req, res) => {
  // Verify that no data is received

  req.on('data', common.mustNotCall());

  req.on('end', common.mustNotCall(() => {
    res.writeHead(200, { 'Content-Type': 'text/plain' });
    res.end();
  }));
}, 1));

server.listen(0, common.mustSucceed(() => {
  const client = net.connect(server.address().port, 'localhost');

  let response = '';

  client.on('data', common.mustCall((chunk) => {
    response += chunk;
  }));

  client.setEncoding('utf8');
  client.on('error', common.mustNotCall());
  client.on('end', common.mustCall(() => {
    assert.strictEqual(
      response,
      'HTTP/1.1 400 Bad Request\r\nConnection: close\r\n\r\n'
    );
    server.close();
  }));
  client.write(msg);
  client.resume();
}));
