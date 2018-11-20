'use strict';
const common = require('../common');
const assert = require('assert');
const http = require('http');

// This test assesses whether long-running writes can complete
// or timeout because the socket is not aware that the backing
// stream is still writing.

const writeSize = 3000000;
let socket;

const server = http.createServer(common.mustCall((req, res) => {
  server.close();
  const content = Buffer.alloc(writeSize, 0x44);

  res.writeHead(200, {
    'Content-Type': 'application/octet-stream',
    'Content-Length': content.length.toString(),
    'Vary': 'Accept-Encoding'
  });

  socket = res.socket;
  const onTimeout = socket._onTimeout;
  socket._onTimeout = common.mustCallAtLeast(() => onTimeout.call(socket), 1);
  res.write(content);
  res.end();
}));
server.on('timeout', () => {
  // TODO(apapirovski): This test is faulty on certain Windows systems
  // as no queue is ever created
  assert(!socket._handle || socket._handle.writeQueueSize === 0,
         'Should not timeout');
});

server.listen(0, common.mustCall(() => {
  http.get({
    path: '/',
    port: server.address().port
  }, (res) => {
    res.once('data', () => {
      socket._onTimeout();
      res.on('data', () => {});
    });
    res.on('end', () => server.close());
  });
}));
