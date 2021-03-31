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
    // TODO: is this test needed?
    // It errors with ERR_HTTP2_NO_SOCKET_MANIPULATION because we are
    // calling destroy on the Proxy(ed) socket of the Http2ClientSession
    // which causes `emit` to becalled and the error to be thrown.
    net.Socket.prototype.destroy.call(client.socket);
    server.close();
  }));
  req.end();
}));
