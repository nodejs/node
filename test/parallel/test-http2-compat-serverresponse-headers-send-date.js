'use strict';
const common = require('../common');
if (!common.hasCrypto) { common.skip('missing crypto'); }
const assert = require('assert');
const http2 = require('http2');

const server = http2.createServer(common.mustCall((request, response) => {
  response.sendDate = false;
  response.writeHead(200);
  response.end();
}));

server.listen(0, common.mustCall(() => {
  const session = http2.connect(`http://localhost:${server.address().port}`);
  const req = session.request();

  req.on('response', common.mustCall((headers, flags) => {
    assert.strictEqual('Date' in headers, false);
    assert.strictEqual('date' in headers, false);
  }));

  req.on('end', common.mustCall(() => {
    session.close();
    server.close();
  }));
}));
