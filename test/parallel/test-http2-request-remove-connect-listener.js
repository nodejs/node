'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const assert = require('assert');
const http2 = require('http2');


const server = http2.createServer();
server.on('stream', common.mustCall((stream) => {
  stream.respond();
  stream.end();
}));

server.listen(0, common.mustCall(() => {
  const client = http2.connect(`http://localhost:${server.address().port}`);
  const req = client.request();
  client.once('connect', common.mustCall());

  req.on('response', common.mustCall(() => {
    assert.strictEqual(client.listenerCount('connect'), 0);
  }));
  req.on('close', common.mustCall(() => {
    server.close();
    client.close();
  }));
}));
