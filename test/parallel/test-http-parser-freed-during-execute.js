'use strict';

const common = require('../common');
const { createServer } = require('http');
const { connect } = require('net');

// Regression test: ensure llhttp_execute() is aborted when freeParser() is
// called synchronously during parsing of pipelined requests.
const server = createServer(common.mustCall((req, res) => {
  req.socket.emit('close');
  res.end();
}, 1));

server.unref();

server.listen(0, common.mustCall(() => {
  // Two pipelined requests in one write, processed by a single llhttp_execute().
  const client = connect(server.address().port);
  client.end(
    'GET / HTTP/1.1\r\nHost: localhost\r\nConnection: keep-alive\r\n\r\n' +
    'GET / HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n',
  );
}));
