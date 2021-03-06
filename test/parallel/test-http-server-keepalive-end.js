'use strict';
const common = require('../common');
const assert = require('assert');
const { createServer } = require('http');
const { connect } = require('net');

// Make sure that for HTTP keepalive requests, the .on('end') event is emitted
// on the incoming request object, and that the parser instance does not hold
// on to that request object afterwards.

const server = createServer(common.mustCall((req, res) => {
  req.on('end', common.mustCall(() => {
    const parser = req.socket.parser;
    assert.strictEqual(parser.incoming, req);
    process.nextTick(() => {
      assert.strictEqual(parser.incoming, null);
    });
  }));
  res.end('hello world');
}));

server.unref();

server.listen(0, common.mustCall(() => {
  const client = connect(server.address().port);

  const req = [
    'POST / HTTP/1.1',
    `Host: localhost:${server.address().port}`,
    'Connection: keep-alive',
    'Content-Length: 11',
    '',
    'hello world',
    ''
  ].join('\r\n');

  client.end(req);
}));
