'use strict';
const common = require('../common');
if (!common.hasCrypto) { common.skip('missing crypto'); }
const assert = require('assert');
const http2 = require('http2');

const server = http2.createServer(common.mustCall((req, res) => {
  const hwm = req.socket.writableHighWaterMark;
  assert.strictEqual(res.writableHighWaterMark, hwm);
  assert.strictEqual(res.writableLength, 0);
  res.write('');
  const len = res.writableLength;
  res.write('asd');
  assert.strictEqual(res.writableLength, len + 3);
  res.end();
  res.on('finish', common.mustCall(() => {
    assert.strictEqual(res.writableLength, 0);
    assert.ok(res.writableFinished, 'writableFinished is not truthy');
    server.close();
  }));
}));

server.listen(0, common.mustCall(() => {
  const client = http2.connect(`http://localhost:${server.address().port}`);
  const request = client.request();
  request.on('data', common.mustCall());
  request.on('end', common.mustCall(() => {
    client.close();
  }));
}));
