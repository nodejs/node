'use strict';
const common = require('../common');
const { createServer } = require('http');
const { connect } = require('net');

// Make sure that calling the semi-private close() handlers manually doesn't
// cause an error.

const server = createServer(common.mustCall((req, res) => {
  req.client._events.close.forEach((fn) => { fn.bind(req)(); });
}));

server.unref();

server.listen(0, common.mustCall(() => {
  const client = connect(server.address().port);

  const req = [
    'POST / HTTP/1.1',
    'Host: example.com',
    'Content-Length: 11',
    '',
    'hello world',
  ].join('\r\n');

  client.end(req);
}));
