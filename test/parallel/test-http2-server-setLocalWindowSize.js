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
  const defaultSetting = http2.getDefaultSettings();
  session.setLocalWindowSize(windowSize);

  assert.strictEqual(session.state.effectiveLocalWindowSize, windowSize);
  assert.strictEqual(session.state.localWindowSize, windowSize);
  assert.strictEqual(
    session.state.remoteWindowSize,
    defaultSetting.initialWindowSize
  );
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
