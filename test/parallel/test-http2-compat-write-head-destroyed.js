'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const http2 = require('http2');

// Check that writeHead, write and end do not crash in compatibility mode

const server = http2.createServer(common.mustCall((req, res) => {
  // destroy the stream first
  req.stream.destroy();

  res.writeHead(200);
  res.write('hello ');
  res.end('world');
}));

server.listen(0, common.mustCall(() => {
  const client = http2.connect(`http://localhost:${server.address().port}`);

  const req = client.request();
  req.on('response', common.mustNotCall());
  req.on('close', common.mustCall((arg) => {
    client.close();
    server.close();
  }));
  req.resume();
}));
