// Flags: --expose-http2
'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const assert = require('assert');
const http2 = require('http2');

let client;
let req;
const server = http2.createServer();
server.on('stream', common.mustCall((stream) => {
  stream.on('error', common.mustCall(() => {
    stream.on('close', common.mustCall((code) => {
      assert.strictEqual(code, 2);
      client.destroy();
      server.close();
    }));
  }));

  req.rstStream(2);
}));
server.listen(0, common.mustCall(() => {
  client = http2.connect(`http://localhost:${server.address().port}`);
  req = client.request();
  req.resume();
  req.on('error', common.mustCall());
}));
