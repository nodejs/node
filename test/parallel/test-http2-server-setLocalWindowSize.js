'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const http2 = require('http2');

const server = http2.createServer();
server.on('stream', common.mustCall((stream) => {
  stream.respond();
  stream.end('ok');
}));
server.on('session', common.mustCall((session) => {
  const windowSize = 2 ** 20;
  session.setLocalWindowSize(windowSize);

  assert.strictEqual(session.state.effectiveLocalWindowSize, windowSize);
  // localWindowSize returns the available connection window.
  // When decreasing from the default 33554432 to 1048576,
  // the available window stays at 33554432.
  assert.strictEqual(session.state.localWindowSize, 33554432);
  // remoteWindowSize is the connection-level send window,
  // which remains at the HTTP/2 default of 65535.
  assert.strictEqual(session.state.remoteWindowSize, 65535);
}));

server.listen(0, common.mustCall(() => {
  const client = http2.connect(`http://localhost:${server.address().port}`);

  const req = client.request();
  req.resume();
  req.on('close', common.mustCall(() => {
    client.close();
    server.close();
  }));
}));
