'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const assert = require('assert');
const http2 = require('http2');

const server = http2.createServer();
const data = Buffer.from([0x1, 0x2, 0x3, 0x4, 0x5]);
let session;

server.on('stream', common.mustCall((stream) => {
  session = stream.session;
  session.on('close', common.mustCall());
  session.goaway(0, 0, data);
  stream.respond();
  stream.end();
}));
server.on('close', common.mustCall());

server.listen(0, () => {
  const client = http2.connect(`http://localhost:${server.address().port}`);
  client.once('goaway', common.mustCall((code, lastStreamID, buf) => {
    assert.strictEqual(code, 0);
    assert.strictEqual(lastStreamID, 1);
    assert.deepStrictEqual(data, buf);
    session.close();
    server.close();
  }));
  const req = client.request();
  req.resume();
  req.on('end', common.mustCall());
  req.on('close', common.mustCall());
  req.end();
});
