'use strict';

const common = require('../common');

const http = require('http');
const net = require('net');

const msg = [
  'POST / HTTP/1.1',
  'Host: 127.0.0.1',
  'Transfer-Encoding: chunked',
  'Transfer-Encoding: chunked-false',
  'Connection: upgrade',
  '',
  '1',
  'A',
  '0',
  '',
  'GET /flag HTTP/1.1',
  'Host: 127.0.0.1',
  '',
  '',
].join('\r\n');

// Verify that the server is called only once even with a smuggled request.

const server = http.createServer(common.mustCall((req, res) => {
  res.end();
}, 1));

function send(next) {
  const client = net.connect(server.address().port, 'localhost');
  client.setEncoding('utf8');
  client.on('error', common.mustNotCall());
  client.on('end', next);
  client.write(msg);
  client.resume();
}

server.listen(0, common.mustSucceed(() => {
  send(common.mustCall(() => {
    server.close();
  }));
}));
