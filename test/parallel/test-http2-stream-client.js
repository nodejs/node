'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const assert = require('assert');
const http2 = require('http2');
const util = require('util');

const server = http2.createServer();
server.on('stream', common.mustCall((stream) => {
  assert.strictEqual(stream.aborted, false);
  const insp = util.inspect(stream);
  assert.ok(/Http2Stream { id/.test(insp));
  assert.ok(/ {2}state:/.test(insp));
  assert.ok(/ {2}readableState:/.test(insp));
  assert.ok(/ {2}writableState:/.test(insp));
  stream.end('ok');
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
