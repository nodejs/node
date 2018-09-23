'use strict';

const {
  hasCrypto,
  mustCall,
  skip
} = require('../common');
if (!hasCrypto)
  skip('missing crypto');
const {
  createServer,
  connect
} = require('http2');
const assert = require('assert');

const server = createServer();
server.on('stream', mustCall((stream) => {
  assert(stream.session.getLastReceivedTime() > 0);
  stream.respond();
  stream.end('ok');
}));

server.listen(0, mustCall(() => {
  const client = connect(`http://localhost:${server.address().port}`);

  const req = client.request();
  req.on('response', mustCall(() => {
    assert(client.getLastReceivedTime() > 0);
  }));
  req.resume();
  req.on('close', mustCall(() => {
    server.close();
    client.close();
  }));
}));
