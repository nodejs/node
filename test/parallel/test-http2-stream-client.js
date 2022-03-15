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
  assert.match(insp, /Http2Stream {/);
  assert.match(insp, / {2}state:/);
  assert.match(insp, / {2}readableState:/);
  assert.match(insp, / {2}writableState:/);
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
