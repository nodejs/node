'use strict';

const common = require('../common');
const http = require('http');
const net = require('net');
const { finished } = require('stream');

const headers =
  'GET / HTTP/1.1\r\n' +
  'Host: localhost\r\n' +
  'Connection: keep-alive\r\n' +
  'Agent: node\r\n';

const baseTimeout = 1000;

const server = http.createServer(common.mustCall((req, res) => {
  req.resume();
  res.writeHead(200);
  res.end();
}, 2));

server.keepAliveTimeout = 10 * baseTimeout;
server.headersTimeout = baseTimeout;

server.once('timeout', common.mustNotCall((socket) => {
  socket.destroy();
}));

server.listen(0, () => {
  const client = net.connect(server.address().port);

  // first request
  client.write(headers);
  client.write('\r\n');

  setTimeout(() => {
    // second request
    client.write(headers);
    // `headersTimeout` doesn't seem to fire if request
    // is sent altogether.
    setTimeout(() => {
      client.write('\r\n');
      client.end();
    }, 10);
  }, baseTimeout + 10);

  client.resume();
  finished(client, common.mustCall((err) => {
    server.close();
  }));
});
