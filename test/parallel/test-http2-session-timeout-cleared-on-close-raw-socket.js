'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const assert = require('assert');
const http2 = require('http2');
const net = require('net');

// Same as test-http2-session-timeout-cleared-on-close.js, but the client's
// TCP socket is destroyed without sending GOAWAY.
const server = http2.createServer();
server.setTimeout(30_000, common.mustNotCall());

server.on('session', common.mustCall((session) => {
  session.on('goaway', common.mustNotCall());
  session.on('close', common.mustCall(() => {
    assert.strictEqual(session.timeout, 0);
    server.close();
  }));
}));

server.on('stream', common.mustCall((stream) => {
  stream.respond({ ':status': 200 });
  stream.end('ok');
}));

server.listen(0, common.mustCall(() => {
  let socket;
  const client = http2.connect(`http://localhost:${server.address().port}`, {
    createConnection: (url) => (socket = net.connect(url.port, url.hostname)),
  });
  client.on('error', () => {});
  const req = client.request();
  req.on('error', () => {});
  req.resume();
  req.on('end', common.mustCall(() => socket.destroy()));
}));
