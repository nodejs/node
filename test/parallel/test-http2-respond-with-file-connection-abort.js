'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const assert = require('assert');
const http2 = require('http2');
const net = require('net');

const {
  HTTP2_HEADER_CONTENT_TYPE
} = http2.constants;

const server = http2.createServer();
server.on('stream', common.mustCall((stream) => {
  stream.on('error', (err) => assert.strictEqual(err.code, 'ECONNRESET'));
  stream.respondWithFile(process.execPath, {
    [HTTP2_HEADER_CONTENT_TYPE]: 'application/octet-stream'
  });
}));

server.listen(0, common.mustCall(() => {
  const client = http2.connect(`http://localhost:${server.address().port}`);
  const req = client.request();

  req.on('response', common.mustCall(() => {}));
  req.once('data', common.mustCall(() => {
    net.Socket.prototype.destroy.call(client.socket);
    server.close();
  }));
  req.end();
}));
